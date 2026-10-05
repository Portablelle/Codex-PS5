#include "../src/markdown.hpp"
#include <cassert>
#include <iostream>
int main(){
    auto text=assistant::markdown("- **PS5 internal storage** : **510.4 GiB**");
    assert(text.text=="– PS5 internal storage : 510.4 GiB");
    assert(text.styles.size()==text.text.size());
    assert(text.styles[text.text.find("510")]&assistant::bold);
    assert(text.styles[text.text.find(':')]==assistant::plain);
    text=assistant::markdown("# Summary\n`/data/a_b.txt`\n```sh\necho '**text**'\n```\nEnd");
    assert(text.text=="Summary\n/data/a_b.txt\necho '**text**'\nEnd");
    assert(text.styles.front()&assistant::bold);
    assert(text.styles[text.text.find("/data")]&assistant::code);
    text=assistant::markdown("2 * 3, /data/a_b_c.txt, \\*text\\*, [site](https://example.com)");
    assert(text.text=="2 * 3, /data/a_b_c.txt, *text*, site (https://example.com)");
    text=assistant::markdown("**reply in progress");assert(text.text=="**reply in progress");
    std::cout<<"Markdown: storage reply, UTF-8, code, links and literal markers passed\n";
}
