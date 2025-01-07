# ImmHijack
An Ashita v4 plugin that allows the game to process IME input in wide character (UTF-16) APIs instead of the ANSI codepage APIs.

With this plugin, there's no need to change the system locale, and the game can properly handle Japanese input.

Tested with Microsoft IME, Windows 11 24H2, FFXI 2024/12 update.

## How it works
When this plugin is loaded, it hijacks the ```ImmGetCompositionStringA``` and ```ImmGetCandidateListA``` functions which is originally called by FFXI. Instead, the plugin calls ```ImmGetCompositionStringW``` and ```ImmGetCandidateListW```, then converts the retrieved string(s) to SJIS using ```WideCharToMultiByte```. Since this approach get rid of functions that use ANSI codepage, it is locale-independent, meaning users don't need to modify their system settings.

## Trivials
Technically, it is possible to make this a standalone application or a Windower plugin. However, I couldn't find a Windower plugin API/SDK, and I personally prefer using Ashita.
