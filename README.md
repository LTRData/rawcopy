# rawcopy

A Windows command-line utility for copying bytes between files, disk devices, pipes and standard input/output. It supports partial copies, input and output offsets, configurable buffering, sparse output and updating only output blocks that differ.

The Visual C++ project builds [rawcopy.cpp](rawcopy.cpp). The other `rawcopy*.cpp` files are retained variants and are not compiled by that project.

## Usage

```text
rawcopy [options]
rawcopy [options] outfile
rawcopy [options] infile outfile
rawcopy [options] copylength infile outfile
rawcopy [options] skipforward copylength infile outfile
```

Put options before the positional arguments. With no positional arguments, rawcopy copies standard input to standard output; with one, it copies standard input to the named output. Use an empty filename (`""`) for standard input or output in the other forms. Use `rawcopy -?` to display help.

Lengths and offsets are in bytes. Uppercase suffixes such as `K`, `M` and `G` use powers of 1024; lowercase `k`, `m` and `g` use powers of 1000. Lengths and offsets also accept `B` for 512-byte blocks. An omitted or zero copy length means copy until the input ends. Input skipping and `-o` move relative to the corresponding handle's current position.

**Existing output files are overwritten without being automatically truncated.** If an old destination is longer than the copied data, its trailing bytes remain. Use a fresh output filename for a new copy.

### Examples

The following examples use Windows Command Prompt (`cmd.exe`) syntax and assume the output files do not already exist.

Copy a file with a 512 KiB buffer and progress messages:

```bat
rawcopy -m -v source.bin new-copy.bin
```

Copy the first 1 MiB, or skip 512 bytes and then copy 4096 bytes:

```bat
rawcopy 1M source.bin first-megabyte.bin
rawcopy 512 4096 source.bin section.bin
```

Read 512 bytes from the beginning of a physical disk:

```bat
rawcopy 512 \\.\PhysicalDrive0 first-sector.bin
```

Copy standard input to a file using shell redirection:

```bat
rawcopy stdin-copy.bin < source.bin
```

Raw disk or volume access may require an elevated command prompt. Verify the device identity and direction of the copy: writing to a disk or volume directly can destroy its contents. Rawcopy attempts to lock and dismount device volumes, including input volumes, so device reads can also disrupt a mounted filesystem.

### Options

| Option | Effect |
| --- | --- |
| `-m` / `-m:size` | Use a 512 KiB buffer, or the specified size. Without this option, the buffer is 512 bytes. |
| `-v` | Report progress to standard error, leaving standard output available for copied data. |
| `-o:offset` | Skip forward in the output before writing. |
| `-D` | Compare against existing output data and write only differing blocks. Requires a readable, seekable output; it does not create a separate difference image. |
| `-s` | Request sparse output and skip zero blocks beyond the output's original length, where the filesystem supports it. |
| `-a` | Adjust output length to the detected input volume size, accounting for input skipping. Requires a disk-volume input. |
| `-r` / `-w` | Use unbuffered input / output. Buffer sizes, lengths and offsets must meet the device's alignment requirements. |
| `-x` | Use write-through output. |
| `-d` | Request extended DASD I/O on the handles. Use a buffer size compatible with the device. |
| `-b` | Enable available backup privileges and open handles with backup semantics. This does not grant privileges the account lacks. |
| `-f:count` | Retry failed I/O, waiting 500 ms between attempts. A count of zero selects 10 retries. |
| `-i` | Ignore I/O errors without prompting. Ignored reads are replaced with zero-filled blocks; ignored writes skip the unwritten region. |
| `-l` | Allow continuing when a volume lock fails. The lock/dismount attempt still occurs. |

Options are case-sensitive: `-D` and `-d` have different meanings.

## Building

Open [rawcopy.sln](rawcopy.sln) in Visual Studio with the C++ build tools. The checked-in project selects Windows SDK **10.0.19041.0** and these platform toolsets:

| Platform | Toolset |
| --- | --- |
| Win32 | v141 |
| x64 | v143 |
| ARM | v120 |
| ARM64 | v141 |

The build expects sibling checkouts of [include](https://github.com/LTRData/include), [libsrc](https://github.com/LTRData/libsrc) and [props](https://github.com/LTRData/props). These supply shared headers, the `wconmsgw.cpp`, `woemprnf.c` and `wwperror.c` helper sources, and imported property sheets.

Release configurations also import `signltr.props` from an absolute path in the maintainer's environment. Adapt that signing import for a local build. The repository is therefore not a self-contained Visual Studio build; install the selected toolset and arrange the shared dependencies first.

## Credits

By Olof Lagerkvist, LTR Data. The command-line help carries a 1997–2023 copyright notice and credits LZ for the modification on which differential operation is based.
