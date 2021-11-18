#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <winstrct.h>
#include <winioctl.h>
#include <shellapi.h>
#include <ntdll.h>

#include "rawcopy.rc.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ntdll.lib")

#define STDBUFSIZ	512
#define BIGBUFSIZ	(512 << 10)

union VOLUME_BITMAP_BUFFERS
{
    STARTING_LCN_INPUT_BUFFER input;
    VOLUME_BITMAP_BUFFER output;
    char bytes[1 << 20];
};

char buffer[STDBUFSIZ];
LONGLONG copylength = { 0 };
LONGLONG readbytes = { 0 };
LONGLONG writtenblocks = { 0 };
LONGLONG skippedwritebytes = { 0 };
SIZE_T bufsiz = STDBUFSIZ;

HANDLE hIn = INVALID_HANDLE_VALUE;
HANDLE hOut = INVALID_HANDLE_VALUE;

int
wmain(int argc, WCHAR *argv[])
{
    bool bBigBufferMode = false;	// -m switch
    bool bDeviceLockRequired = true;	// not -l switch
    bool bVerboseMode = false;	// -v switch
    bool bIgnoreErrors = false;	// -i switch
    bool bAdjustSize = false;     // -a switch
    bool bCreateSparse = false;   // -s switch
    bool bDifferential = false;   // -D switch
    bool bNonBufferedIn = false;  // -r switch
    bool bNonBufferedOut = false; // -w switch
    bool bWriteThrough = false;   // -x switch
    bool bExtendedDASDIO = false; // -d switch
    bool bDisplayHelp = false;
    DWORD retrycount = 0;         // -f switch
    SIZE_T sizBigBufferSize = BIGBUFSIZ;	// -m:nnn parameter
    LARGE_INTEGER sizWriteOffset = { 0 };	// -o:nnn parameter

    // Nice argument parse loop :)
    while (argv[1] != NULL && (argv[1][0] | 0x02) == '/')
    {
        while ((++argv[1])[0])
        {
            switch (argv[1][0])
            {
            case L'm':
                bBigBufferMode = true;
                if (argv[1][1] == ':')
                {
                    LPWSTR szSizeSuffix = NULL;
                    sizBigBufferSize = wcstoul(argv[1] + 2, &szSizeSuffix, 0);

                    if (szSizeSuffix == argv[1] + 2 ||
                        (szSizeSuffix[0] != 0 && szSizeSuffix[1] != 0))
                    {
                        fprintf(stderr,
                            "Invalid buffer size: %ws\n", szSizeSuffix);
                        return -1;
                    }

                    switch (szSizeSuffix[0])
                    {
                    case L'E':
                        sizBigBufferSize <<= 10;
                    case L'P':
                        sizBigBufferSize <<= 10;
                    case L'T':
                        sizBigBufferSize <<= 10;
                    case L'G':
                        sizBigBufferSize <<= 10;
                    case L'M':
                        sizBigBufferSize <<= 10;
                    case L'K':
                        sizBigBufferSize <<= 10;
                        break;
                    case L'e':
                        sizBigBufferSize *= 1000;
                    case L'p':
                        sizBigBufferSize *= 1000;
                    case L't':
                        sizBigBufferSize *= 1000;
                    case L'g':
                        sizBigBufferSize *= 1000;
                    case L'm':
                        sizBigBufferSize *= 1000;
                    case L'k':
                        sizBigBufferSize *= 1000;
                    case 0:
                        break;
                    default:
                        fprintf(stderr,
                            "Invalid size suffix: %wc\n", szSizeSuffix[0]);
                        return -1;
                    }

                    argv[1] += wcslen(argv[1]) - 1;
                }
                break;
            case L'o':
            {
                if (argv[1][1] != L':')
                {
                    bDisplayHelp = true;
                    break;
                }

                WCHAR SizeSuffix = NULL;
                switch (swscanf(argv[1] + 2, L"%I64i%c",
                    &sizWriteOffset.QuadPart, &SizeSuffix))
                {
                case 2:
                    switch (SizeSuffix)
                    {
                    case L'E':
                        sizWriteOffset.QuadPart <<= 60;
                        break;
                    case L'P':
                        sizWriteOffset.QuadPart <<= 50;
                        break;
                    case L'T':
                        sizWriteOffset.QuadPart <<= 40;
                        break;
                    case L'G':
                        sizWriteOffset.QuadPart <<= 30;
                        break;
                    case L'M':
                        sizWriteOffset.QuadPart <<= 20;
                        break;
                    case L'K':
                        sizWriteOffset.QuadPart <<= 10;
                        break;
                    case L'e':
                        sizWriteOffset.QuadPart *= 1000000000000000000;
                        break;
                    case L'p':
                        sizWriteOffset.QuadPart *= 1000000000000000;
                        break;
                    case L't':
                        sizWriteOffset.QuadPart *= 1000000000000;
                        break;
                    case L'g':
                        sizWriteOffset.QuadPart *= 1000000000;
                        break;
                    case L'm':
                        sizWriteOffset.QuadPart *= 1000000;
                        break;
                    case L'k':
                        sizWriteOffset.QuadPart *= 1000;
                        break;
                    case L'B':
                        sizWriteOffset.QuadPart *= 512;
                        break;
                    case 0:
                        break;
                    default:
                        fprintf(stderr,
                            "Invalid forward skip suffix: %wc\n",
                            SizeSuffix);
                        return -1;
                    }

                case 1:
                    break;

                default:
                    fprintf(stderr, "Invalid skip size: %ws\n", argv[1]);
                    return -1;
                }

                if (bVerboseMode)
                    printf("Write starts at %I64i bytes.\n", sizWriteOffset.QuadPart);
            }

            argv[1] += wcslen(argv[1]) - 1;

            break;
            case L'f':
                if (argv[1][1] != ':')
                {
                    bDisplayHelp = true;
                    break;
                }

                retrycount = wcstoul(argv[1] + 2, NULL, 0);
                if (retrycount == 0)
                    retrycount = 10;

                argv[1] += wcslen(argv[1]) - 1;

                break;
            case L'l':
                bDeviceLockRequired = false;
                break;
            case L'i':
                bIgnoreErrors = true;
                break;
            case L'v':
                bVerboseMode = true;
                break;
            case L'a':
                bAdjustSize = true;
                break;
            case L's':
                bCreateSparse = true;
                break;
            case L'D':
                bDifferential = true;
                break;
            case L'r':
                bNonBufferedIn = true;
                break;
            case L'w':
                bNonBufferedOut = true;
                break;
            case L'x':
                bWriteThrough = true;
                break;
            case L'd':
                bExtendedDASDIO = true;
                break;
            default:
                bDisplayHelp = true;
            }
        }

        --argc;
        ++argv;
    }

    if (bIgnoreErrors)
    {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    }

    if (bDisplayHelp)
    {
        fputs("File and device read/write utility.\n"
            "\n"
            "Version " RAWCOPY_VERSION ". "
            "Copyright (C) Olof Lagerkvist 1997-2018.\n"
            "Differential operation based on modification by LZ.\n"
            "This program is open source freeware.\n"
            "http://www.ltr-data.se      olof@ltr-data.se\n"
            "\n"
            "Usage:\n"
            "rawcopy [-lvirwxsdD] [-f[:count]] [-m[:bufsize]] [-o:writeoffset]\n"
            "       [[[[[skipforward] copylength] infile] outfile]\n"
            "\n"
            "Default infile/outfile if none or blank given is standard input/output device.\n"
            "-l     Access devices without locking access to them. Use this switch when you\n"
            "       are reading/writing physical drives and other processes are using the\n"
            "       drives.\n"
            "       Note! This may destroy your data!\n"
            "\n"
            "-v     Verbose mode. Writes to stderr what is being done.\n"
            "\n"
            "-f     Number of retries on failed I/O operations.\n"
            "\n"
            "-m     Try to buffer more of input file into memory before writing to output\n"
            "       file.\n"
            "\n"
            "       Buffer size may be suffixed with K,M or G. If -m is given without a\n"
            "       buffer size, 512 KB is assumed. If -m is not given a buffer size of 512\n"
            "       bytes is used and read/write failures can be ignored.\n"
            "\n"
            "-i     Ignores and skip over read/write failures without displaying any dialog\n"
            "       boxes.\n"
            "\n"
            "-r     Read input without intermediate buffering.\n"
            "\n"
            "-w     Write output without intermediate buffering.\n"
            "\n"
            "-x     Write through to output without going via system cache.\n"
            "\n"
            "-s     Creates output file as sparse file on NTFS volumes and skips explicitly\n"
            "       writing all-zero blocks.\n"
            "\n"
            "-D     Differential operation. Skips rewriting blocks in output file that are\n"
            "       already equal to corresponding blocks in input file.\n"
            "\n"
            "-d     Use extended DASD I/O when reading/writing disks. You also need to\n"
            "       select a compatible buffer size with the -m switch for successful\n"
            "       operation.\n"
            "\n"
            "-a     Adjusts size out output file to disk volume size of input. This switch\n"
            "       is only valid if input is a disk volume.\n"
            "\n"
            "Examples for Windows NT:\n"
            "rawcopy -m diskimage.img \\\\.\\A:\n"
            "  Writes a diskette image file called \"diskimage.img\" to a physical diskette\n"
            "  in drive A:. (File extension .img is just an example, sometimes such images\n"
            "  are called for example .vfd.)\n"
            "rawcopy \\\\.\\PIPE\\MYPIPE \"\"\n"
            "  Reads data from named pipe \"MYPIPE\" on the local machine and write on\n"
            "  standard output.\n"
            "rawcopy 512 \\\\.\\PhysicalDrive0 parttable\n"
            "  Copies 512 bytes (partition table) from first physical harddrive to file\n"
            "  called \"parttable\".\n", stderr);

        return -1;
    }

    LARGE_INTEGER skipforward = { 0 };
    if (argc > 4)
    {
        WCHAR SizeSuffix = 0;
        switch (swscanf(argv[1], L"%I64i%c",
            &skipforward.QuadPart, &SizeSuffix))
        {
        case 2:
            switch (SizeSuffix)
            {
            case L'E':
                skipforward.QuadPart <<= 60;
                break;
            case L'P':
                skipforward.QuadPart <<= 50;
                break;
            case L'T':
                skipforward.QuadPart <<= 40;
                break;
            case L'G':
                skipforward.QuadPart <<= 30;
                break;
            case L'M':
                skipforward.QuadPart <<= 20;
                break;
            case L'K':
                skipforward.QuadPart <<= 10;
                break;
            case L'e':
                skipforward.QuadPart *= 1000000000000000000;
                break;
            case L'p':
                skipforward.QuadPart *= 1000000000000000;
                break;
            case L't':
                skipforward.QuadPart *= 1000000000000;
                break;
            case L'g':
                skipforward.QuadPart *= 1000000000;
                break;
            case L'm':
                skipforward.QuadPart *= 1000000;
                break;
            case L'k':
                skipforward.QuadPart *= 1000;
                break;
            case L'B':
                skipforward.QuadPart *= 512;
                break;
            case 0:
                break;
            default:
                fprintf(stderr,
                    "Invalid forward skip suffix: %wc\n", SizeSuffix);
                return -1;
            }

        case 1:
            break;

        default:
            fprintf(stderr, "Invalid skip size: %ws\n", argv[1]);
            return -1;
        }

        if (bVerboseMode)
            printf("Skipping %I64i bytes.\n", skipforward.QuadPart);

        argv++;
        argc--;
    }

    if (argc > 3)
    {
        WCHAR SizeSuffix = 0;
        switch (swscanf(argv[1], L"%I64i%c",
            &copylength, &SizeSuffix))
        {
        case 2:
            switch (SizeSuffix)
            {
            case 'E':
                copylength <<= 60;
                break;
            case 'P':
                copylength <<= 50;
                break;
            case 'T':
                copylength <<= 40;
                break;
            case 'G':
                copylength <<= 30;
                break;
            case 'M':
                copylength <<= 20;
                break;
            case 'K':
                copylength <<= 10;
                break;
            case 'e':
                copylength *= 1000000000000000000;
                break;
            case 'p':
                copylength *= 1000000000000000;
                break;
            case 't':
                copylength *= 1000000000000;
                break;
            case 'g':
                copylength *= 1000000000;
                break;
            case 'm':
                copylength *= 1000000;
                break;
            case 'k':
                copylength *= 1000;
                break;
            case 'B':
                copylength *= 512;
                break;
            case 0:
                break;
            default:
                fprintf(stderr, "Invalid copylength suffix: %c\n",
                    SizeSuffix);

                return -1;
            }

        case 1:
            break;

        default:
            fprintf(stderr, "Invalid copylength: %ws\n", argv[1]);
            return 1;
        }

        if (bVerboseMode)
            printf("Copying %I64i bytes.\n", copylength);

        argv++;
        argc--;
    }

    if ((argc > 2) && (argv[1] != NULL) && (argv[1][0] != 0))
    {
        hIn = CreateFile(argv[1],
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_SEQUENTIAL_SCAN |
            (bNonBufferedIn ? FILE_FLAG_NO_BUFFERING : 0),
            NULL);
    }
    else
    {
        hIn = GetStdHandle(STD_INPUT_HANDLE);
    }

    if (hIn == INVALID_HANDLE_VALUE)
    {
        win_perror(argc > 1 ? argv[1] : L"stdin");
        if (bVerboseMode)
            fputs("Error opening input file.\n", stderr);
        return -1;
    }

    if (skipforward.QuadPart &&
        (SetFilePointer(hIn, skipforward.LowPart, &skipforward.HighPart,
            FILE_CURRENT) == INVALID_SET_FILE_POINTER) &&
            (GetLastError() != NO_ERROR))
    {
        win_perror(L"Fatal, cannot set input file pointer");
        return -1;
    }

    if (bExtendedDASDIO)
    {
        // Turn on FSCTL_ALLOW_EXTENDED_DASD_IO so that we can make sure that we
        // read the entire drive bypassing filesystem limits
        DWORD dwReadSize;
        DeviceIoControl(hIn,
            FSCTL_ALLOW_EXTENDED_DASD_IO,
            NULL,
            0,
            NULL,
            0,
            &dwReadSize,
            NULL);
    }

    LARGE_INTEGER disk_size = { 0 };
    if (bAdjustSize)
    {
        // Get volume size
        DWORD dwBytesReturned;
        DeviceIoControl(hIn,
            IOCTL_DISK_UPDATE_PROPERTIES,
            NULL,
            0,
            NULL,
            0,
            &dwBytesReturned,
            NULL);

        PARTITION_INFORMATION partition_info = { 0 };
        DISK_GEOMETRY disk_geometry = { 0 };
        if (DeviceIoControl(hIn,
            IOCTL_DISK_GET_PARTITION_INFO,
            NULL,
            0,
            &partition_info,
            sizeof(partition_info),
            &dwBytesReturned,
            NULL))
        {
            disk_size.QuadPart = partition_info.PartitionLength.QuadPart;
        }
        else if (DeviceIoControl(hIn,
            IOCTL_DISK_GET_DRIVE_GEOMETRY,
            NULL,
            0,
            &disk_geometry,
            sizeof(disk_geometry),
            &dwBytesReturned,
            NULL))
        {
            disk_size.QuadPart =
                disk_geometry.Cylinders.QuadPart *
                disk_geometry.TracksPerCylinder *
                disk_geometry.SectorsPerTrack *
                disk_geometry.BytesPerSector;
        }
        else
        {
            win_perror(L"Cannot get input disk volume size");
            return 1;
        }
    }

    char *bufptr = NULL;
    char *diffbufptr = NULL;

    // If -m option is used we use big buffer
    if (bBigBufferMode)
    {
        if ((bufptr = (char *)LocalAlloc(LPTR, sizBigBufferSize)) != NULL)
        {
            bufsiz = LocalSize(bufptr);
        }
        else
        {
            bufptr = buffer;
            win_perror(L"Memory allocation error, "
                L"using 512 bytes buffer instead%%nError message");
        }

        if (bVerboseMode)
        {
            fprintf(stderr, "Buffering %u bytes.\n", (DWORD)bufsiz);
        }
    }
    // -m is not used
    else
    {
        bufptr = buffer;
        if (bVerboseMode)
        {
            fprintf(stderr,
                "Sequential scan mode used, "
                "max read/write buffer is %u bytes.\n", (DWORD)bufsiz);
        }
    }

    if (bDifferential)
    {
        diffbufptr = (char*)LocalAlloc(LPTR, bufsiz);
        if (diffbufptr == NULL)
        {
            win_perror(L"Memory allocation error");
            return -1;
        }
    }

    // Jump to next parameter if input file was given
    if (argc > 2)
    {
        argv++;
        argc--;
    }

    if ((argc > 1) && (argv[1] != NULL) && (argv[1][0] != 0))
    {
        hOut = CreateFile(argv[1],
            (bDifferential ? GENERIC_READ : 0) | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL |
            (bNonBufferedOut ? FILE_FLAG_NO_BUFFERING : 0) |
            (bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),
            hIn);

        if (hOut == INVALID_HANDLE_VALUE &&
            GetLastError() == ERROR_INVALID_PARAMETER)
        {
            hOut = CreateFile(argv[1],
                (bDifferential ? GENERIC_READ : 0) | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                (bNonBufferedOut ? FILE_FLAG_NO_BUFFERING : 0) |
                (bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),
                NULL);
        }
    }
    else
    {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    if (hOut == INVALID_HANDLE_VALUE)
    {
        win_perror(argc > 1 ? argv[1] : L"stdout");

        if (bVerboseMode)
            fputs("Error opening output file.\n", stderr);

        return -1;
    }

    if (bExtendedDASDIO)
    {
        // Turn on FSCTL_ALLOW_EXTENDED_DASD_IO so that we can make sure that we
        // read the entire drive
        DWORD dwReadSize;
        DeviceIoControl(hOut,
            FSCTL_ALLOW_EXTENDED_DASD_IO,
            NULL,
            0,
            NULL,
            0,
            &dwReadSize,
            NULL);
    }

    // If not regular disk file, try to lock volume using FSCTL operation
    BY_HANDLE_FILE_INFORMATION ByHandleFileInfo;
    if (!GetFileInformationByHandle(hIn, &ByHandleFileInfo))
    {
        DWORD z;
        FlushFileBuffers(hIn);
        if (DeviceIoControl(hIn, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &z, NULL))
        {
            if (DeviceIoControl(hIn, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &z,
                NULL))
            {
                if (bVerboseMode)
                    fputs("Source device locked and dismounted.\n", stderr);
            }
            else
            {
                if (bVerboseMode)
                    fputs("Source device locked.\n", stderr);
            }
        }
        else
        {
            switch (GetLastError())
            {
            case ERROR_NOT_SUPPORTED:
            case ERROR_INVALID_FUNCTION:
            case ERROR_INVALID_HANDLE:
            case ERROR_INVALID_PARAMETER:
                break;

            default:
                if (bDeviceLockRequired)
                {
                    if (bVerboseMode)
                    {
                        DWORD dwSavedErrno = GetLastError();
                        fprintf(stderr, "System Error %u.\n", dwSavedErrno);
                        SetLastError(dwSavedErrno);
                    }

                    win_perror(L"Cannot lock source device (use -l to ignore)");
                    return -1;
                }
                else
                    win_perror(L"Warning! Source device not locked");
            }
        }
    }

    LARGE_INTEGER existing_out_file_size = { 0 };
    if (GetFileInformationByHandle(hOut, &ByHandleFileInfo))
    {
        existing_out_file_size.LowPart = ByHandleFileInfo.nFileSizeLow;
        existing_out_file_size.HighPart = ByHandleFileInfo.nFileSizeHigh;

        LARGE_INTEGER current_out_pointer = { 0 };
        current_out_pointer.LowPart =
            SetFilePointer(hOut, 0, &current_out_pointer.HighPart, FILE_CURRENT);
        if (current_out_pointer.LowPart == INVALID_SET_FILE_POINTER &&
            GetLastError() != NO_ERROR)
        {
            win_perror(L"Fatal, cannot get output file pointer");
            return -1;
        }

        existing_out_file_size.QuadPart -= current_out_pointer.QuadPart;
    }
    else
    {
        DWORD z;
        FlushFileBuffers(hOut);

        if (DeviceIoControl(hOut, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &z, NULL))
        {
            if (DeviceIoControl(hOut, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &z,
                NULL))
            {
                if (bVerboseMode)
                    fputs("Target device locked and dismounted.\n", stderr);
            }
            else
            {
                if (bVerboseMode)
                    fputs("Target device locked.\n", stderr);
            }
        }
        else
        {
            switch (GetLastError())
            {
            case ERROR_NOT_SUPPORTED:
            case ERROR_INVALID_FUNCTION:
            case ERROR_INVALID_HANDLE:
            case ERROR_INVALID_PARAMETER:
                break;

            default:
                if (bDeviceLockRequired)
                {
                    if (bVerboseMode)
                    {
                        DWORD dwSavedErrno = GetLastError();
                        fprintf(stderr, "System Error %u.\n", dwSavedErrno);
                        SetLastError(dwSavedErrno);
                    }

                    win_perror(L"Cannot lock target device (use -l to ignore)");
                    return -1;
                }
                else
                    win_perror(L"Warning! Target device not locked");
            }
        }
    }

    if (bCreateSparse)
    {
        DWORD dw;

        if (!DeviceIoControl(hOut, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dw,
            NULL))
        {
            win_perror(L"Warning! Failed to set sparse flag for file");
        }
    }

    if (sizWriteOffset.QuadPart != 0)
    {
        sizWriteOffset.LowPart = SetFilePointer(
            hOut,
            sizWriteOffset.LowPart,
            &sizWriteOffset.HighPart,
            FILE_CURRENT);

        if (sizWriteOffset.LowPart == INVALID_SET_FILE_POINTER &&
            GetLastError() != NO_ERROR)
        {
            win_perror(L"Error seeking on output device");
            return 2;
        }
    }

    for (;;)
    {
        Sleep(0);

        if (bVerboseMode)
            fprintf(stderr, "Reading block %I64u\r", writtenblocks);

        // Read next block
        DWORD dwBlockSize = (DWORD)bufsiz;
        DWORD retries = 0;

        if (copylength != 0)
        {
            if (readbytes >= copylength)
            {
                break;
            }
            else if ((dwBlockSize + readbytes) >= copylength)
            {
                dwBlockSize = (DWORD)(copylength - readbytes);
                if (bVerboseMode)
                {
                    fprintf(stderr, "Reading last %u bytes...\n", dwBlockSize);
                }
            }
        }

        DWORD dwReadSize = 0;

        while (!ReadFile(hIn, bufptr, dwBlockSize, &dwReadSize, NULL))
        {
            DWORD dwErrNo = GetLastError();

            // End of physical disk? Might need to read fewer bytes towards end of device.
            if (dwErrNo == ERROR_SECTOR_NOT_FOUND &&
                dwReadSize == 0 &&
                dwBlockSize > 512)
            {
                dwBlockSize = 512;
                continue;
            }

            // End of pipe, disk etc?
            if (dwReadSize == 0 &&
                (dwErrNo == ERROR_BROKEN_PIPE ||
                    dwErrNo == ERROR_INVALID_PARAMETER ||
                    dwErrNo == ERROR_SECTOR_NOT_FOUND))
            {
                break;
            }

            WErrMsg errmsg;

            if (retries < retrycount)
            {
                fprintf(stderr, "Rawcopy read failure: %ws\nRetrying...\n",
                    (LPCWSTR)errmsg);

                retries++;

                Sleep(500);

                continue;
            }

            retries = 0;

            switch (bIgnoreErrors ? IDIGNORE :
                MessageBox(NULL, errmsg, L"Rawcopy read failure",
                    MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION |
                    MB_DEFBUTTON2 | MB_TASKMODAL))
            {
            case IDABORT:
                return -1;
            case IDRETRY:
                continue;
            case IDIGNORE:
                if (bVerboseMode)
                {
                    fprintf(stderr, "Ignoring read error at block %I64u: %ws\n",
                        writtenblocks, (LPCWSTR)errmsg);
                }

                dwReadSize = dwBlockSize;
                ZeroMemory(bufptr, dwReadSize);
                LARGE_INTEGER newpos = { 0 };
                newpos.QuadPart = bufsiz;
                if (SetFilePointer(hIn, newpos.LowPart, &newpos.HighPart,
                    FILE_CURRENT) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR)
                {
                    win_perror(L"Fatal, cannot set input file pointer");
                    return -1;
                }
            }
            break;
        }

        // Check for EOF condition
        if (dwReadSize == 0)
        {
            if (bVerboseMode)
                fputs("\r\nEnd of input.\n", stderr);

            break;
        }

        // Check for all-zeros block and skip explicitly writing it to output
        // file if "create sparse" flag is set and we are writing outside
        // output file current length
        bool bSkipWriteBlock = false;

        if (bCreateSparse && readbytes >= existing_out_file_size.QuadPart &&
            RtlCompareMemoryUlong(bufptr, dwReadSize, 0) == dwReadSize)
        {
            bSkipWriteBlock = true;
        }

        readbytes += dwReadSize;

        // Check existing block in output file and skip explicitly writing it
        // again if "differential" flag is set and output block is already equal
        // to corresponding block in input file.
        if (bDifferential && !bSkipWriteBlock)
        {
            DWORD dwDiffReadSize;

            if (!ReadFile(hOut, diffbufptr, dwReadSize, &dwDiffReadSize, NULL))
            {
                win_perror(L"Error reading output file");
                return -1;
            }

            if (dwDiffReadSize == dwReadSize)
            {
                LARGE_INTEGER rewind_pos = { 0 };
                rewind_pos.QuadPart = -(LONGLONG)dwDiffReadSize;
                if (SetFilePointer(hOut,
                    rewind_pos.LowPart,
                    &rewind_pos.HighPart,
                    FILE_CURRENT) == INVALID_SET_FILE_POINTER &&
                    GetLastError() != NO_ERROR)
                {
                    win_perror(L"Fatal, cannot set output file pointer");
                    return -1;
                }

                if (memcmp(bufptr, diffbufptr, dwReadSize) == 0)
                {
                    bSkipWriteBlock = true;
                }
            }
        }

        DWORD dwWriteSize = 0;
        if (bSkipWriteBlock)
        {
            if (bVerboseMode)
                fprintf(stderr, "Skipped block %I64u\r", writtenblocks);

            LONG lZero = 0;
            if (SetFilePointer(hOut, dwReadSize, &lZero, FILE_CURRENT) ==
                INVALID_SET_FILE_POINTER &&
                GetLastError() != NO_ERROR)
            {
                win_perror(L"Fatal, cannot set output file pointer");
                return -1;
            }

            if (readbytes > existing_out_file_size.QuadPart)
            {
                SetEndOfFile(hOut);
            }

            dwWriteSize = dwReadSize;

            skippedwritebytes += dwWriteSize;
        }
        else
        {
            if (bVerboseMode)
                fprintf(stderr, "Writing block %I64u\r", writtenblocks);

            DWORD write_retries = 0;

            // Write next block
            while (!WriteFile(hOut, bufptr, dwReadSize, &dwWriteSize, NULL))
            {
                DWORD dwErrNo = GetLastError();
                if ((dwErrNo == ERROR_BROKEN_PIPE) ||
                    (dwErrNo == ERROR_INVALID_PARAMETER))
                    break;

                WErrMsg errmsg;

                if (write_retries < retrycount)
                {
                    fprintf(stderr, "Rawcopy write failure: %ws\nRetrying...\n",
                        (LPCWSTR)errmsg);

                    write_retries++;
                    Sleep(500);
                    continue;
                }

                write_retries = 0;

                switch (bIgnoreErrors ? IDIGNORE :
                    MessageBox(NULL, errmsg, L"Rawcopy write failure",
                        MB_ABORTRETRYIGNORE | MB_ICONEXCLAMATION |
                        MB_DEFBUTTON2 | MB_TASKMODAL))
                {
                case IDABORT:
                    return -1;
                case IDRETRY:
                    continue;
                case IDIGNORE:
                    if (bVerboseMode)
                    {
                        fprintf(stderr,
                            "Ignoring write error at block %I64u: %ws\n",
                            writtenblocks, (LPCWSTR)errmsg);
                    }

                    LONG lZero = 0;
                    if (SetFilePointer(hOut, dwReadSize - dwWriteSize, &lZero,
                        FILE_CURRENT) == INVALID_SET_FILE_POINTER &&
                        GetLastError() != NO_ERROR)
                    {
                        win_perror(L"Fatal, cannot set output file pointer");
                        return -1;
                    }
                }
                break;
            }

            // Check for EOF condition
            if (dwWriteSize == 0)
                break;
        }

        if (dwWriteSize < dwReadSize)
        {
            fprintf(stderr, "Warning: %u bytes lost.\n",
                dwReadSize - dwWriteSize);
        }

        // Check for EOF condition
        if (dwWriteSize == 0)
        {
            if (bVerboseMode)
                fputs("\r\nEnd of output.\n", stderr);

            break;
        }

        ++writtenblocks;
        if (copylength != 0 &&
            writtenblocks >= copylength)
        {
            break;
        }
    }

    if (bAdjustSize)
    {
        if (disk_size.QuadPart > skipforward.QuadPart)
        {
            disk_size.QuadPart -= skipforward.QuadPart;
        }

        ULARGE_INTEGER existing_size = { 0 };
        DWORD ptr;

        // This piece of code compares size of created image file with that of
        // the original disk volume and possibly adjusts image file size if it
        // does not exactly match the size of the original disk/partition.
        existing_size.LowPart = GetFileSize(hOut, &existing_size.HighPart);
        if (existing_size.LowPart == INVALID_FILE_SIZE &&
            GetLastError() != NO_ERROR)
        {
            win_perror(L"Error getting output file size");
            return 1;
        }

        if (existing_size.QuadPart < (ULONGLONG)disk_size.QuadPart)
        {
            fprintf(stderr,
                "Warning: Output file size is smaller "
                "than input disk volume size.\n"
                "Input disk volume size: %I64i bytes.\n"
                "Output image file size: %I64i bytes.\n",
                disk_size.QuadPart, existing_size.QuadPart);

            return 1;
        }

        ptr = SetFilePointer(hOut,
            disk_size.LowPart,
            (LPLONG)&disk_size.HighPart,
            FILE_BEGIN);

        if (ptr == INVALID_SET_FILE_POINTER &&
            GetLastError() != NO_ERROR)
        {
            win_perror(L"Error setting output file size");
            return 1;
        }

        if (!SetEndOfFile(hOut))
        {
            win_perror(L"Error setting output file size");
            return 1;
        }
    }

    if (bDifferential || bCreateSparse)
    {
        LONGLONG updated_bytes;
        updated_bytes = readbytes - skippedwritebytes;
        fprintf(stderr, "\n%.4g %s of %.4g %s updated (%.3g %%).\n",
            TO_h(updated_bytes), TO_p(updated_bytes),
            TO_h(readbytes), TO_p(readbytes),
            (double)(100 * (double)updated_bytes /
                readbytes));
    }
    else if (bVerboseMode)
    {
        fprintf(stderr, "\n%.4g %s copied.\n",
            TO_h(readbytes), TO_p(readbytes));
    }
}

#if _MSC_VER < 1900

// This is enough startup code for this program if compiled to use the DLL CRT.
extern "C"
int
wmainCRTStartup()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    exit(wmain(argc, argv));
}

#endif
