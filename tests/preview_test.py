#!/usr/bin/env python3
"""Layout checks on host preview frames (build/preview, raw PPM, no Pillow)."""
import os, subprocess, sys, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def render(**env):
    with tempfile.TemporaryDirectory() as directory:
        path = os.path.join(directory, 'frame.ppm')
        subprocess.run([os.path.join(ROOT, 'build/preview')], cwd=ROOT, check=True,
                       env={**os.environ, 'PREVIEW_OUTPUT': path, **env})
        data = open(path, 'rb').read()
    header = b'P6\n1920 1080\n255\n'
    assert data.startswith(header) and len(data) == len(header) + 1920 * 1080 * 3
    return data[len(header):]

def region(frame, x0, y0, x1, y1):
    return b''.join(frame[(y * 1920 + x0) * 3:(y * 1920 + x1) * 3] for y in range(y0, y1))

def lit(pixels):
    return sum(1 for i in range(0, len(pixels), 3) if max(pixels[i:i + 3]) > 128)

# The device code and URL stay visible while the controller keyboard is open.
closed = render(PREVIEW_SIGNIN='1')
assert lit(region(closed, 96, 168, 1000, 500)) > 2000
opened = render(PREVIEW_SIGNIN='1', PREVIEW_KEYBOARD='1')
assert lit(region(opened, 96, 168, 1000, 500)) > 2000, 'sign-in code hidden by keyboard layout'

# A long server status never draws over the model label or past the margin.
short = render(PREVIEW_STATUS='Ready')
error = 'Error: Your access token could not be refreshed because your refresh token was already used. Please log out and sign in again.'
long = render(PREVIEW_STATUS=error)
assert region(short, 1590, 50, 1920, 110) == region(long, 1590, 50, 1920, 110), 'status overlaps model label'
# Failing statuses are still shown in full above the composer.
assert lit(region(long, 96, 760, 1824, 860)) > lit(region(short, 96, 760, 1824, 860)) + 2000
print('Preview: sign-in with keyboard, long status clipping and error notice passed')
