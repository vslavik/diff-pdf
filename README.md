# diff-pdf

[![Build](https://github.com/emilianbold/diff-pdf/actions/workflows/build.yml/badge.svg)](https://github.com/emilianbold/diff-pdf/actions/workflows/build.yml)

A command-line tool for visually comparing two PDF files on Linux and macOS.

## Example

<img src="samples/diff-pdf-demo.png" alt="diff-pdf output showing visual differences between two PDFs" height="400">

The tool highlights differences between PDFs, making it easy to spot changes in text, colors, and layout.

## Usage

```bash
# Compare two PDFs (exit code 0 if identical, 1 if different)
diff-pdf file1.pdf file2.pdf

# Generate a visual diff PDF
diff-pdf --output-diff=diff.pdf file1.pdf file2.pdf

# Skip identical pages in output
diff-pdf --skip-identical --output-diff=diff.pdf file1.pdf file2.pdf

# Verbose mode
diff-pdf --verbose file1.pdf file2.pdf
```

See `diff-pdf --help` for all options.

## Installation

### Binary releases

Precompiled binaries for Linux and macOS are available from [GitHub releases](https://github.com/emilianbold/diff-pdf/releases).

**Runtime dependencies:**
```bash
# Ubuntu/Debian
sudo apt-get install libpoppler-glib8 libcairo2

# Fedora/CentOS
sudo dnf install poppler-glib cairo
```

The macOS binary is standalone — it bundles all required libraries and does not require Homebrew at runtime. Extract the tarball and run `diff-pdf` directly.

### Build from source

```bash
# Ubuntu/Debian dependencies
sudo apt-get install make automake g++ libpoppler-glib-dev libcairo2-dev pkg-config

# macOS dependencies (Homebrew)
brew install automake autoconf pkg-config poppler cairo glib

# Build and install
./bootstrap
./configure
make
sudo make install
```

On macOS, if `./configure` cannot find the libraries, set `PKG_CONFIG_PATH` first:
```bash
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:$PKG_CONFIG_PATH"
```

Dependencies: Cairo >= 1.4, Poppler >= 0.10, GLib >= 2.36
