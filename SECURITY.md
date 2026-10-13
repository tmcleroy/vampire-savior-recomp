# Security

The program reads files the player chooses: a ROM set (a folder or a zip), `settings.ini`, save
states, input movies and the board's settings memory. A file that makes it read or write outside
its own memory, or write a file anywhere but the player's data folder, is a real bug and worth a
private report. The launcher starts the game program that sits beside it and nothing else.

Please report such a thing privately through GitHub ("Report a vulnerability" on the repository's
Security tab) or to tmcleroy@gmail.com, and not in a public issue.
