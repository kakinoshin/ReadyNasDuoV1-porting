# Repository layout

This is a personal project. Nothing is planned for submission upstream, and everything is
kept in the owner's GitHub account. The NetBSD fork is public, because GitHub forks of
public repositories cannot be private.

| Target | How it is managed |
|---|---|
| `docs/`, `hw/`, `tools/`, `toolchain/*.sh` | This top-level repository |
| `src/netbsd-src` | Public GitHub fork of `NetBSD/src`, referenced as a submodule |
| GCC / binutils (`sparc-elf`) | Unmodified; versions pinned in `toolchain/build-sparc-elf.sh` |
| `refs/`, build outputs | Not tracked |

## NetBSD

- `origin` is the owner's fork; `upstream` is `https://github.com/NetBSD/src.git`.
- `trunk` follows upstream; port work happens on the `padre` branch.
- Changes to NetBSD's in-tree GCC / binutils are kept in this repository too.

## GCC / binutils

When the `sparc-elf` sources need changes, fork upstream gcc / binutils-gdb the same way
and add them as submodules. Patches on top of release tarballs are not used.

## Conventions

- Code comments, documentation and commit messages are in English.
- Git operations are never automatic: each one needs prior approval.
