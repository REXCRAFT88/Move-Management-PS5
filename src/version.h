/* The one place AnyPad PS5's version is set. Every ELF carries it in its name
 * (dist/AnyPad-PS5-<version>.elf), in its log's first line, in the console
 * notification and on the web page. Raise it with every build that leaves
 * this folder, and write what changed in CHANGELOG.md. */
#ifndef ANYPAD_VERSION_H
#define ANYPAD_VERSION_H

#ifdef MOVE_STUDIO_VIRTUAL_BETA
#define ANYPAD_VERSION "0.1.0-beta6.12-virtual-external"
#elif defined(MOVE_STUDIO_EXTERNAL_BT)
#define ANYPAD_VERSION "0.1.0-beta6.12-external"
#else
#define ANYPAD_VERSION "0.1.0-beta6.12-internal-experimental"
#endif

#endif
