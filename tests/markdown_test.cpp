#include "../src/markdown.hpp"
#include <cassert>
#include <iostream>
int main(){
    auto text=assistant::markdown("- **Stockage interne PS5** : **510,4 Gio**");
    assert(text.text=="– Stockage interne PS5 : 510,4 Gio");
    assert(text.styles.size()==text.text.size());
    assert(text.styles[text.text.find("510")]&assistant::bold);
    assert(text.styles[text.text.find(':')]==assistant::plain);
    text=assistant::markdown("# Résumé\n`/data/a_b.txt`\n```sh\necho '**texte**'\n```\nFin");
    assert(text.text=="Résumé\n/data/a_b.txt\necho '**texte**'\nFin");
    assert(text.styles.front()&assistant::bold);
    assert(text.styles[text.text.find("/data")]&assistant::code);
    text=assistant::markdown("2 * 3, /data/a_b_c.txt, \\*texte\\*, [site](https://example.com)");
    assert(text.text=="2 * 3, /data/a_b_c.txt, *texte*, site (https://example.com)");
    text=assistant::markdown("**réponse en cours");assert(text.text=="**réponse en cours");
    std::cout<<"Markdown: storage reply, UTF-8, code, links and literal markers passed\n";
}
