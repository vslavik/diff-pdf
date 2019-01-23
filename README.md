*Note: this repository is provided **as-is** and the code is not being actively
developed. If you wish to improve it, that's greatly appreciated: please make
the changes and submit a pull request, I'll gladly merge it or help you out
with finishing it. However, please do not expect any kind of support, including
implementation of feature requests or fixes. If you're not a developer and/or
willing to get your hands dirty, this tool is probably not for you.*

## Usage

diff-pdf is a tool for visually comparing two PDFs.

It takes two PDF files as arguments. By default, its only output is its return
code, which is 0 if there are no differences and 1 if the two PDFs differ. If
given the `--output-diff` option, it produces a PDF file with visually
highlighted differences:

```
$ diff-pdf --output-diff=diff.pdf a.pdf b.pdf
```

Another option is to compare the two files visually in a simple GUI, using
the `--view` argument:

```
$ diff-pdf --view a.pdf b.pdf
```

This opens a window that lets you view the files' pages and zoom in on details.
It is also possible to shift the two pages relatively to each other using
Ctrl-arrows. This is useful for identifying translation-only differences.

See the output of `$ diff-pdf --help` for complete list of options.


## Obtaining the binaries

Precompiled version of the tool for Windows is available from our
[downloads](http://www.tt-solutions.com/downloads/diff-pdf-2012-02-28.zip)
section -- the ZIP archive contains everything you need to run diff-pdf. It will
work from any place you unpack it to.

On Mac, if you use [Homebrew](https://brew.sh), you can use it to install diff-pdf with it:
```
$ brew install diff-pdf
```

Precompiled version for openSUSE and Fedora can be downloaded from the
[openSUSE build service](http://software.opensuse.org).


## Compiling from sources

The build system uses Automake and so a Unix or Unix-like environment (Cygwin
or MSYS) is required. Compilation is done in the usual way:

```
$ ./bootstrap
$ ./configure
$ make
$ make install
```

(Note that the first step, running the `./bootstrap` script, is only required
when building sources checked from version control system, i.e. when `configure`
and `Makefile.in` files are missing.)

As for dependencies, diff-pdf requires the following libraries:

- wxWidgets >= 3.0
- Cairo >= 1.4
- Poppler >= 0.10

#### CentOS:

```
$ sudo yum groupinstall "Development Tools"
$ sudo yum install wxGTK wxGTK-devel poppler-glib poppler-glib-devel
```

#### Ubuntu:

```
$ sudo apt-get install make automake g++
$ sudo apt-get install libpoppler-glib-dev poppler-utils wxgtk3.0-dev
```

#### OSX:
Install Command Line Tools for Xcode:

```
$ xcode-select --install
```

and install [Homebrew](https://brew.sh) or [MacPorts](https://www.macports.org) to manage dependencies, then:

```
$ brew install automake autoconf wxmac poppler cairo
```

or

```
$ sudo port install automake autoconf wxWidgets-3.0 poppler cairo
```

Note that many more libraries are required on Windows, where none of the
libraries Cairo and Poppler use are normally available. At the time of writing,
transitive cover of the above dependencies included fontconfig, freetype, glib,
libpng, pixman, gettext, libiconv, libjpeg and zlib.

Precompiled version of the dependencies for Windows, built with MinGW, are
available from our [downlods](http://github.com/vslavik/diff-pdf/downloads)
section (wxWidgets is not included, as it supports MinGW well).


### Compiling on Windows using MinGW

1. First of all, you will need working MinGW installation with MSYS environment
and C++ compiler. The instructions on installing MinGW can be found in the
[MinGW website](http://www.mingw.org/wiki/Getting_Started):

1. Once installed, launch the MinGW Shell tool. It will open a terminal window;
type `$ cd /c/directory/with/diff-pdf` to do to the directory with diff-pdf
sources.

1. You will need to install additional MinGW components that are not normally
included with MinGW, using these commands:

    ```
    $ mingw-get install msys-wget
    $ mingw-get install msys-unzip
    $ mingw-get install msys-perl
    ```

1. It's time to build the software now and give it some time to finish (Note
that this step requires Internet connectivity, as it downloads some dependencies
packages):

    ```
    $ make -f Makefile.mingw
    ```

1. If everything went well, diff-pdf is now in the `mingw\Products\`
directory.


## Installing

On Unix, the usual `make install` is sufficient.

On Windows, installation is not necessary, just copy the files somewhere. If
you built it following the instructions above, all the necessary files will be
in the `mingw\Products\` directory.
