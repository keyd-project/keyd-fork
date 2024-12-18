[![Kofi](https://badgen.net/badge/icon/kofi?icon=kofi&label)](https://ko-fi.com/rvaiya)
[![Packaging status](https://repology.org/badge/tiny-repos/keyd.svg)](https://repology.org/project/keyd/versions)

# keyd (temporary friendly fork)

A key remapping daemon for Linux.

> [!NOTE]
> https://github.com/keyd-project/keyd-fork is a temporary, friendly fork of the
> [original keyd project](https://github.com/rvaiya/keyd) (upstream).  This fork
> was created on 2024-12-06 with the following goals:
>
>   * Help reduce the backlog of pull requests in the original project by
>     merging them here and creating releases that users can test.
>   * Provide a well-maintained distribution tarball to make it easier for
>     downstream packagers to maintain keyd packages.
>   * Recruit potential maintainers.  (If you are interested, please speak up at
>     [rvaiya/keyd#887](https://github.com/rvaiya/keyd/issues/887) or contact
>     [@rhansen](https://github.com/rhansen) directly.)
>
> Once the original project acquires a vibrant group of maintainers, this fork
> will be archived.  If the original project is abandoned, this fork will be
> made permanent by dropping the `-fork` suffix.
>
> See [rvaiya/keyd#887](https://github.com/rvaiya/keyd/issues/887) for
> discussion.
>
> This fork's branch plan: Commits intended to be reintegrated with the upstream
> project will be put on a `reintegrate-vN` branch where `N` is a version
> number.  If new commits are pushed to the upstream project, and the new
> commits cause the current `reintegrate-vN` branch to no longer be
> fast-forwardable from the upstream's default branch:
>
>   1. A new `reintegrate-vM` branch (where `M = N+1`) is created pointing to
>      the same commit as `reintegrate-vN`, but not pushed yet.
>   2. `reintegrate-vM` is rebased onto the upstream project's default branch,
>      preserving merges.
>   3. `reintegrate-vM` is merged into `main`.
>   4. `reintegrate-vN` is deleted.
>
> This is a bit painful, but the goal is to make it as easy as possible to
> reintegrate with the upstream project without force-pushing this fork's `main`
> branch.
>
> Feel free to open issues or pull requests in this fork.  Pull requests
> containing commits intended for reintegration into the upstream project should
> be merged into the current `reintegrate-vN` branch, not `main`.  After the
> pull request is merged, `reintegrate-vN` is merged into `main`.

## Impetus

Linux lacks a good key remapping solution. In order to achieve satisfactory
results a medley of tools need to be employed (e.g xcape, xmodmap) with the end
result often being tethered to a specified environment (X11). keyd attempts to
solve this problem by providing a flexible system wide daemon which remaps keys
using kernel level input primitives (evdev, uinput).

## Note on v2

The config format has undergone several iterations since the first
release. For those migrating their configs from v1 it is best
to reread the man page. 

See also: [changelog](docs/CHANGELOG.md).

## Goals

  - Speed       (a hand tuned input loop written in C that takes <<1ms)
  - Simplicity  (a [config format](#sample-config) that is intuitive)
  - Consistency (modifiers that [play nicely with
    layers](https://github.com/keyd-project/keyd-fork/blob/6dc2d5c4ea76802fd192b143bdd53b1787fd6deb/docs/keyd.scdoc#L128)
    by default)
  - Modularity (a UNIXy core extensible through the use of an
    [IPC](https://github.com/keyd-project/keyd-fork/blob/90973686723522c2e44d8e90bb3508a6da625a20/docs/keyd.scdoc#L391)
    mechanism)

## Features

keyd has several unique features many of which are traditionally only
found in custom keyboard firmware like [QMK](https://github.com/qmk/qmk_firmware)
as well as some which are unique to keyd.

Some of the more interesting ones include:

- Layers (with support for [hybrid
  modifiers](https://github.com/keyd-project/keyd-fork/blob/6dc2d5c4ea76802fd192b143bdd53b1787fd6deb/docs/keyd.scdoc#L128)).
- Key overloading (different behaviour on tap/hold).
- Keyboard specific configuration.
- Instantaneous remapping (no more flashing :)).
- A client-server model that facilitates scripting and display server agnostic application remapping. (Currently ships with support for X, sway, and gnome (wayland)).
- System wide config (works in a VT).
- First class support for modifier overloading.
- Unicode support.

### keyd is for people who:

 - Would like to experiment with custom [layers](https://docs.qmk.fm/feature_layers) (i.e custom shift keys)
   and oneshot modifiers.
 - Want to have multiple keyboards with different layouts on the same machine.
 - Want to be able to remap `C-1` without breaking modifier semantics.
 - Want a keyboard config format which is easy to grok.
 - Like tiny daemons that adhere to the Unix philosophy.
 - Want to put the control and escape keys where God intended.
 - Wish to be able to switch to a VT to debug something without breaking their keymap.

### What keyd isn't:

 - A tool for programming individual key up/down events.

## Dependencies

### Required Build Dependencies

  * Your favourite C11 compiler
  * POSIX-compatible `make` (e.g., [GNU
    Make](https://www.gnu.org/software/make/))
  * Linux kernel headers (already present on most systems)
  * [Python](https://www.python.org/) 3.8 or newer
  * [scdoc](https://git.sr.ht/~sircmpwn/scdoc)

### Optional Build Dependencies

  * [bash](https://www.gnu.org/software/bash/) (for the `usb-gadget` virtual
    keyboard driver)
  * [systemd](https://systemd.io/) (for system services)

### Optional Runtime Dependencies

  * [bash](https://www.gnu.org/software/bash/) (for the `usb-gadget` virtual
    keyboard driver)
  * [Python](https://www.python.org/) 3.8 or newer (for application specific
    remapping)
  * [python-xlib](https://pypi.org/project/python-xlib/) (only for X support)
  * [dbus-python](https://pypi.org/project/dbus-python/) (only for KDE support)
  * [systemd](https://systemd.io/) (for system services)

## Installation

### From a Pre-Built Package

Binary packages for some distributions exist.  These are kindly maintained by
community members; the keyd developers do not take responsibility for them.  If
you wish to add yours below, please open a PR.

#### Alpine Linux

[keyd](https://pkgs.alpinelinux.org/packages?name=keyd) package maintained by [@jirutka](https://github.com/jirutka).

#### Arch

[Arch Linux](https://archlinux.org/packages/extra/x86_64/keyd/) package maintained by Arch packagers.

#### Debian

Experimental `keyd` and `keyd-application-mapper` packages can be found in the
CI build artifacts of the [work-in-progress Debian package
repository](https://salsa.debian.org/rhansen/keyd):

  * [amd64 (64-bit)](https://salsa.debian.org/rhansen/keyd/-/jobs/artifacts/debian/latest/browse/debian/output?job=build)
  * [i386 (32-bit)](https://salsa.debian.org/rhansen/keyd/-/jobs/artifacts/debian/latest/browse/debian/output?job=build%20i386)

Any Debian Developer who is willing to review the debianization effort and
sponsor its upload is encouraged to contact
[@rhansen](https://github.com/rhansen) (also see the [Debian ITP
bug](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1060023)).

#### Fedora

[COPR](https://copr.fedorainfracloud.org/coprs/alternateved/keyd/) package maintained by [@alternateved](https://github.com/alternateved).

#### openSUSE

[opensuse](https://software.opensuse.org//download.html?project=hardware&package=keyd) package maintained by [@bubbleguuum](https://github.com/bubbleguuum).

Easy install with `sudo zypper in keyd`.

#### Ubuntu

Experimental `keyd` and `keyd-application-mapper` packages can be found in the
[`ppa:keyd-team/ppa`
archive](https://launchpad.net/~keyd-team/+archive/ubuntu/ppa).

If you wish to help maintain this PPA, please contact
[@rhansen](https://github.com/rhansen).

### From Source

> [!NOTE]
> The default branch (`main`) is the development branch; it contains the latest
> work-in-progress code.  Things may occasionally break between releases.
> Stable releases are [tagged](https://github.com/keyd-project/keyd-fork/tags)
> and announced on the [releases
> page](https://github.com/keyd-project/keyd-fork/releases); these versions are
> known to work.

#### From a Source Code Distribution Tarball

  1. Download the desired tarball:
       * Releases can be found on the [releases
         page](https://github.com/keyd-project/keyd-fork/releases).
       * The current revision of the `main` branch (work-in-progress development
         version) can be found at
         <https://nightly.link/keyd-project/keyd-fork/workflows/ci/main/distribution-tarball>.
         Note that the distribution tarball is inside a zip file [due to an
         unfortunate GitHub
         limitation](https://github.com/actions/upload-artifact/issues/426).

  2. Extract the tarball and `cd` to the source code directory:

     ```shell
     tar xvfa /path/to/keyd-*.tar.gz
     cd keyd-*
     ```

  3. Ensure that all [required build dependencies](#required-build-dependencies)
     have been installed.

  4. Build and install keyd:

     ```shell
     ./configure
     make
     sudo make install
     ```

#### From Git

  1. Install [Git](https://git-scm.com/), [GNU
     Autoconf](https://www.gnu.org/software/autoconf/), and [GNU
     Automake](https://www.gnu.org/software/automake/).

  2. Get the source code:

     ```shell
     git clone https://github.com/keyd-project/keyd-fork
     cd keyd-fork
     ```

  3. Optionally switch to a released revision, if desired:

     ```shell
     git checkout v2.5.0
     ```

  4. Generate the `configure` script:

     ```shell
     ./autogen
     ```

  5. Follow steps 3 and later from [installation from a source code distribution
     tarball](#from-a-source-code-distribution-tarball).

## Quickstart

1. Install and start keyd (e.g `sudo systemctl enable keyd --now`)

2. Put the following in `/etc/keyd/default.conf`:

```
[ids]

*

[main]

# Maps capslock to escape when pressed and control when held.
capslock = overload(control, esc)

# Remaps the escape key to capslock
esc = capslock
```

Key names can be obtained by using the `keyd monitor` command. Note that while keyd is running, the output of this
command will correspond to keyd's output. The original input events can be seen by first stopping keyd and then
running the command. See the man page for more details.

3. Run `sudo keyd reload` to reload the config set.

4. See the man page (`man keyd`) for a more comprehensive description.

Config errors will appear in the log output and can be accessed in the usual
way using your system's service manager (e.g `sudo journalctl -eu keyd`).

*Note*: It is possible to render your machine unusable with a bad config file.
Should you find yourself in this position, the special key sequence
`backspace+escape+enter` should cause keyd to terminate.

Some mice (e.g the Logitech MX Master) are capable of emitting keys and
are consequently matched by the wildcard id. It may be necessary to
explicitly blacklist these.

### Application Specific Remapping (experimental)

- Add yourself to the keyd group:

	`usermod -aG keyd <user>`

- Populate `~/.config/keyd/app.conf`:

E.G

	[alacritty]

	alt.] = macro(C-g n)
	alt.[ = macro(C-g p)

	[chromium]

	alt.[ = C-S-tab
	alt.] = macro(C-tab)

- Run:

	`keyd-application-mapper`

You will probably want to put `keyd-application-mapper -d` somewhere in your 
display server initialization logic (e.g ~/.xinitrc) unless you are running Gnome.

See the man page for more details.

### SBC support

Experimental support for single board computers (SBCs) via usb-gadget
has been added courtesy of Giorgi Chavchanidze.

See [usb-gadget.md](src/vkbd/usb-gadget.md) for details.

## Sample Config

	[ids]

	*
	
	[main]

	leftshift = oneshot(shift)
	capslock = overload(symbols, esc)

	[symbols]

	d = ~
	f = /
	...

## Recommended config

Many users will probably not be interested in taking full advantage of keyd.
For those who seek simple quality of life improvements I can recommend the
following config:

	[ids]

	*

	[main]

	shift = oneshot(shift)
	meta = oneshot(meta)
	control = oneshot(control)

	leftalt = oneshot(alt)
	rightalt = oneshot(altgr)

	capslock = overload(control, esc)
	insert = S-insert

This overloads the capslock key to function as both escape (when tapped) and
control (when held) and remaps all modifiers to 'oneshot' keys. Thus to produce
the letter A you can now simply tap shift and then a instead of having to hold
it. Finally it remaps insert to S-insert (paste on X11).

## FAQS

### What about xmodmap/setxkbmap/*?

xmodmap and friends are display server level tools with limited functionality.
keyd is a system level solution which implements advanced features like
layering and [oneshot](https://docs.qmk.fm/#/one_shot_keys)
modifiers.  While some X tools offer similar functionality I am not aware of
anything that is as flexible as keyd.

### What about [kmonad](https://github.com/kmonad/kmonad)?

keyd was written several years ago to allow me to easily experiment with
different layouts on my growing keyboard collection. At the time kmonad did not
exist and custom keyboard firmware like
[QMK](https://github.com/qmk/qmk_firmware) (which inspired keyd) was the only
way to get comparable features. I became aware of kmonad after having published
keyd. While kmonad is a fine project with similar goals, it takes a different
approach and has a different design philosophy.

Notably keyd was written entirely in C with performance and simplicitly in
mind and will likely never be as configurable as kmonad (which is extensible
in Haskell). Having said that, it supplies (in the author's opinion) the
most valuable features in less than 2000 lines of C while providing
a simple language agnostic config format.

### Why doesn't keyd implement feature X?

If you feel something is missing or find a bug you are welcome to file an issue
on github. keyd has a minimalist (but sane) design philosophy which
intentionally omits certain features (e.g execing arbitrary executables
as root). Things which already exist in custom keyboard firmware like QMK are
good candidates for inclusion.

## Contributing

See [CONTRIBUTING](CONTRIBUTING.md).
IRC Channel: #keyd on oftc
