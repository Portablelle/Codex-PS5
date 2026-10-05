SHELL := /bin/bash
.DEFAULT_GOAL := help
.PHONY: help test preview native backend model package
help:
	@echo 'Targets: test preview native backend model package'
	@echo 'native/backend: Docker builds on PS5_BUILD_HOST, then retrieve artifacts'
test:
	bash tools/test.sh
preview:
	bash tools/preview.sh
native:
	bash tools/build-remote.sh
backend:
	bash tools/build-backend-remote.sh
model:
	python3 tools/fetch-model.py
package:
	python3 tools/package.py
