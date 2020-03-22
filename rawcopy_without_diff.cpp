#define WIN32_LEAN_AND_MEAN
#include <stdlib.h>
#include <winstrct.h>
#include <winioctl.h>

/*
#ifdef _DLL
#pragma comment(lib, "minwcrt")
#endif
*/

#define STDBUFSIZ	512
#define BIGBUFSIZ	(512 << 10)

char buffer[STDBUFSIZ];
LARGE_INTEGER copylength = { 0 };
LARGE_INTEGER readbytes = { 0 };
LARGE_INTEGER writtenblocks = { 0 };
DWORD bufsiz = STDBUFSIZ;

HANDLE hIn = INVALID_HANDLE_VALUE, hOut = INVALID_HANDLE_VALUE;

int
main(int argc, char **argv)
{
  bool bBigBufferMode = false;	// -m switch
  bool bDeviceLockRequired = true;	// not -l switch
  bool bVerboseMode = false;	// -v switch
  bool bIgnoreErrors = false;	// -i switch
  bool bCreateSparse = false;   // -s switch
  bool bNonBufferedIn = false;  // -r switch
  bool bNonBufferedOut = false; // -w switch
  bool bWriteThrough = false;   // -x switch
  bool bExtendedDASDIO = false; // -d switch
  bool bDisplayHelp = false;
  DWORD retrycount = 0;         // -f switch
  SIZE_T sizBigBufferSize = BIGBUFSIZ;	// -m:nnn parameter

  // Nice argument parse loop :)
  while (argv[1] ? argv[1][0] ? (argv[1][0] | 0x02) == '/' : false : false)
    {
      while ((++argv[1])[0])
	switch (argv[1][0] | 0x20)
	  {
	  case 'm':
	    bBigBufferMode = true;
	    if (argv[1][1] == ':')
	      {
		char *szSizeSuffix = NULL;
		sizBigBufferSize = strtoul(argv[1] + 2, &szSizeSuffix, 0);
		if (szSizeSuffix == argv[1] + 2)
		  {
		    fprintf(stderr,
			    "Invalid buffer size: %s\r\n", szSizeSuffix);
		    return -1;
		  }

		if (szSizeSuffix[0] ? szSizeSuffix[1] != 0 : false)
		  {
		    fprintf(stderr,
			    "Invalid buffer size: %s\r\n", argv[1] + 2);
		    return -1;
		  }

		switch (szSizeSuffix[0])
		  {
		  case 'G':
		    sizBigBufferSize <<= 10;
		  case 'M':
		    sizBigBufferSize <<= 10;
		  case 'K':
		    sizBigBufferSize <<= 10;
		    break;
		  case 'g':
		    sizBigBufferSize *= 1000;
		  case 'm':
		    sizBigBufferSize *= 1000;
		  case 'k':
		    sizBigBufferSize *= 1000;
		  case 0:
		    break;
		  default:
		    fprintf(stderr,
			    "Invalid size suffix: %c\r\n", szSizeSuffix[0]);
		    return -1;
		  }

		argv[1] += strlen(argv[1]) - 1;
	      }
	    break;
	  case 'f':
	    if (argv[1][1] == ':')
	      {
		retrycount = strtoul(argv[1] + 2, NULL, 0);
		if (retrycount == 0)
		  retrycount = 10;

		argv[1] += strlen(argv[1]) - 1;
	      }
	    break;
	  case 'l':
	    bDeviceLockRequired = false;
	    break;
	  case 'i':
	    bIgnoreErrors = true;
	    break;
	  case 'v':
	    bVerboseMode = true;
	    break;
	  case 's':
	    bCreateSparse = true;
	    break;
	  case 'r':
	    bNonBufferedIn = true;
	    break;
	  case 'w':
	    bNonBufferedOut = true;
	    break;
	  case 'x':
	    bWriteThrough = true;
	    break;
	  case 'd':
	    bExtendedDASDIO = true;
	    break;
	  default:
	    bDisplayHelp = true;
	  }

      --argc;
      ++argv;
    }

  if (bIgnoreErrors)
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

  if (bDisplayHelp)
    {
      puts("File and device read/write utility.\r\n"
	   "\n"
	   "Version 1.2.5. Copyright (C) Olof Lagerkvist 1997-2011.\r\n"
	   "This program is open source freeware.\r\n"
	   "http://www.ltr-data.se      olof@ltr-data.se\r\n"
	   "\n"
	   "Usage:\r\n"
	   "rawcopy [-lvirwxsd] [-f[:count]] [-m[:bufsize]]\r\n"
	   "       [[[[[skipforward] copylength] infile] outfile]\r\n"
	   "\n"
	   "Default infile/outfile if none or blank given is standard input/output device.\r\n"
	   "-l     Access devices without locking access to them. Use this switch when you\r\n"
	   "       are reading/writing physical drives and other processes are using the\r\n"
	   "       drives.\r\n"
	   "       Note! This may destroy your data!\r\n"
	   "\n"
	   "-v     Verbose mode. Writes to stderr what is being done.\r\n"
	   "\n"
	   "-f     Number of retries on failed I/O operations.\r\n"
	   "\n"
	   "-m     Try to buffer more of input file into memory before writing to output\r\n"
	   "       file. It is not possible to ignore read/write failures if this switch is\r\n"
	   "       used.\r\n"
	   "\n"
	   "       Buffer size may be suffixed with K,M or G. If -m is given without a\r\n"
	   "       buffer size, 512 KB is assumed. If -m is not given a buffer size of 512\r\n"
	   "       bytes is used and read/write failures can be ignored.\r\n"
	   "\n"
	   "-i     Ignores and skip over read/write failures without displaying any dialog\r\n"
	   "       boxes.\r\n"
	   "\n"
	   "-r     Read input without intermediate buffering.\r\n"
	   "\n"
	   "-w     Write output without intermediate buffering.\r\n"
	   "\n"
	   "-x     Write through to output without going via system cache.\r\n"
	   "\n"
	   "-s     Creates output file as sparse file on NTFS volumes.\r\n"
	   "\n"
	   "-d     Use extended DASD I/O when reading/writing disks. You also need to\n"
	   "       select a compatible buffer size with the -m switch for successful\r\n"
	   "       operation.\r\n"
	   "\n"
	   "Examples for Windows NT:\r\n"
	   "rawcopy -m diskimage.img \\\\.\\A:\r\n"
	   "  Writes a diskette image file called \"diskimage.img\" to a physical diskette\r\n"
	   "  in drive A:.\r\n"
	   "rawcopy \\\\.\\PIPE\\MYPIPE \"\"\r\n"
	   "  Reads data from named pipe \"MYPIPE\" on the local machine and write on\r\n"
	   "  standard output.\r\n"
	   "rawcopy 1 \\\\.\\PhysicalDrive0 parttable\r\n"
	   "  Copies sector 1 (partition table) from first physical harddrive to file\r\n"
	   "  called \"parttable\".");

      return 0;
    }

  LARGE_INTEGER skipforward = { 0 };
  if (argc > 4)
    {
      char SizeSuffix;
      switch (sscanf(argv[1], "%I64i%c",
		     &skipforward, &SizeSuffix))
	{
	case 2:
	  switch (SizeSuffix)
	    {
	    case 'G':
	      skipforward.QuadPart <<= 10;
	    case 'M':
	      skipforward.QuadPart <<= 10;
	    case 'K':
	      skipforward.QuadPart <<= 10;
	      break;
	    case 'g':
	      skipforward.QuadPart *= 1000;
	    case 'm':
	      skipforward.QuadPart *= 1000;
	    case 'k':
	      skipforward.QuadPart *= 1000;
	    case 0:
	      break;
	    default:
	      fprintf(stderr,
		      "Invalid forward skip suffix: %c\r\n", SizeSuffix);
	      return -1;
	    }

	case 1:
	  break;

	default:
	  fprintf(stderr, "Invalid skip size: %s\n", argv[1]);
	  return -1;
	}

      if (bVerboseMode)
	printf("Skipping %I64i bytes.\n", skipforward);

      argv++;
      argc--;
    }

  if (argc > 3)
    {
      char SizeSuffix;
      switch (sscanf(argv[1], "%I64i%c",
		     &copylength, &SizeSuffix))
	{
	case 2:
	  switch (SizeSuffix)
	    {
	    case 'G':
	      copylength.QuadPart <<= 10;
	    case 'M':
	      copylength.QuadPart <<= 10;
	    case 'K':
	      copylength.QuadPart <<= 10;
	      break;
	    case 'g':
	      copylength.QuadPart *= 1000;
	    case 'm':
	      copylength.QuadPart *= 1000;
	    case 'k':
	      copylength.QuadPart *= 1000;
	    case 0:
	      break;
	    default:
	      fprintf(stderr, "Invalid copylength suffix: %c\r\n",
		      SizeSuffix);
	      return -1;
	    }

	case 1:
	  break;

	default:
	  fprintf(stderr, "Invalid copylength: %s\n", argv[1]);
	  return 1;
	}

      if (bVerboseMode)
	printf("Copying %I64i bytes.\n", copylength);

      argv++;
      argc--;
    }

  if (argc > 2 ? argv[1][0] : false)
    hIn = CreateFile(argv[1],
		     GENERIC_READ,
		     FILE_SHARE_READ | FILE_SHARE_WRITE,
		     NULL,
		     OPEN_EXISTING,
		     FILE_FLAG_SEQUENTIAL_SCAN |
		     (bNonBufferedIn ? FILE_FLAG_NO_BUFFERING : 0),
		     NULL);
  else
    hIn = hStdIn;

  if (hIn == INVALID_HANDLE_VALUE)
    {
      win_perror(argc > 1 ? argv[1] : "stdin");
      return -1;
    }

  if (skipforward.QuadPart)
    {
      if (SetFilePointer(hIn, skipforward.LowPart, &skipforward.HighPart,
			 FILE_CURRENT) == 0xFFFFFFFF)
	if (win_errno != NO_ERROR)
	  {
	    win_perror("SetFilePointer()");
	    return -1;
	  }
    }

  if (bExtendedDASDIO)
    {
      // Turn on FSCTL_ALLOW_EXTENDED_DASD_IO so that we can make sure that we
      // read the entire drive
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

  char *bufptr;

  // If -m option is used we use big buffer
  if (bBigBufferMode)
    {
      if ((bufptr = (char *)LocalAlloc(LPTR, sizBigBufferSize)) != NULL)
	bufsiz = LocalSize(bufptr);
      else
	{
	  bufptr = buffer;
	  win_perror("Memory allocation error, "
		     "using 512 bytes buffer instead%%nError message");
	}

      if (bVerboseMode)
	fprintf(stderr, "Buffering %u bytes.\n", bufsiz);
    }
  // -m is not used
  else
    {
      bufptr = buffer;
      if (bVerboseMode)
	fprintf(stderr,
		"Sequential scan mode used, "
		"max read/write buffer is %u bytes.\n", bufsiz);
    }

  // Jump to next parameter if input file was given
  if (argc > 2)
    {
      argv++;
      argc--;
    }

  if (argc > 1 ? argv[1][0] : false)
    hOut = CreateFile(argv[1],
		      GENERIC_WRITE,
		      FILE_SHARE_READ | FILE_SHARE_WRITE,
		      NULL,
		      OPEN_ALWAYS,
		      FILE_ATTRIBUTE_NORMAL |
		      (bNonBufferedOut ? FILE_FLAG_NO_BUFFERING : 0) |
		      (bWriteThrough ? FILE_FLAG_WRITE_THROUGH : 0),
		      hIn);
  else
    hOut = hStdOut;

  if (hOut == INVALID_HANDLE_VALUE)
    {
      win_perror(argc > 1 ? argv[1] : "stdout");
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
	if (DeviceIoControl(hIn, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &z,
			    NULL))
	  {
	    if (bVerboseMode)
	      fputs("Source device locked and dismounted.\r\n", stderr);
	  }
	else
	  {
	    if (bVerboseMode)
	      fputs("Source device locked.\r\n", stderr);
	  }
      else
	switch (win_errno)
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
		    DWORD dwSavedErrno = win_errno;
		    fprintf(stderr, "System Error %u.\n", dwSavedErrno);
		    win_errno = dwSavedErrno;
		  }

		win_perror("Cannot lock source device (use -l to ignore)");
		return -1;
	      }
	    else
	      win_perror("Warning! Source device not locked");
	  }
    }

  if (!GetFileInformationByHandle(hOut, &ByHandleFileInfo))
    {
      DWORD z;
      FlushFileBuffers(hOut);

      if (DeviceIoControl(hOut, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &z, NULL))
	if (DeviceIoControl(hOut, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &z,
			    NULL))
	  {
	    if (bVerboseMode)
	      fputs("Target device locked and dismounted.\r\n", stderr);
	  }
	else
	  {
	    if (bVerboseMode)
	      fputs("Target device locked.\r\n", stderr);
	  }
      else
	switch (win_errno)
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
		    DWORD dwSavedErrno = win_errno;
		    fprintf(stderr, "System Error %u.\n", dwSavedErrno);
		    win_errno = dwSavedErrno;
		  }

		win_perror("Cannot lock target device (use -l to ignore)");
		return -1;
	      }
	    else
	      win_perror("Warning! Target device not locked");
	  }
    }

  if (bCreateSparse)
    {
      DWORD dw;

      if (!DeviceIoControl(hOut, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &dw,
			   NULL))
	win_perror("Warning! Failed to set sparse flag for file");
    }

  for (;;)
    {
      Sleep(0);

      if (bVerboseMode)
	fprintf(stderr, "Reading block %I64u\r", writtenblocks);

      // Read next block
      DWORD dwBlockSize = bufsiz;
      DWORD retries = 0;

      if (copylength.QuadPart != 0)
	if (readbytes.QuadPart >= copylength.QuadPart)
	  return 0;
	else if ((dwBlockSize + readbytes.QuadPart) >= copylength.QuadPart)
	  {
	    dwBlockSize = (DWORD)(copylength.QuadPart - readbytes.QuadPart);
	    if (bVerboseMode)
	      fprintf(stderr, "Reading last %u bytes...\r", dwBlockSize);
	  }

      DWORD dwReadSize;
      while (!ReadFile(hIn, bufptr, dwBlockSize, &dwReadSize, NULL))
	{
	  // End of pipe?
	  if (win_errno == ERROR_BROKEN_PIPE)
	    return 0;

	  WErrMsg errmsg;

	  if (retries < retrycount)
	    {
	      fprintf(stderr, "Rawcopy read failure: %s\nRetrying...\n",
		      (char*)errmsg);
	      retries++;
	      Sleep(500);
	      continue;
	    }

	  retries = 0;

	  switch (bIgnoreErrors ? IDIGNORE :
		  MessageBox(NULL, errmsg, "Rawcopy read failure",
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
		  CharToOem(errmsg, errmsg);
		  fprintf(stderr, "Skipping block %I64u: %s\r\n",
			  writtenblocks, errmsg);
		}

	      dwReadSize = dwBlockSize;
	      ZeroMemory(bufptr, dwReadSize);
	      LONG lZero = 0;
	      if (SetFilePointer(hIn, bufsiz, &lZero, FILE_CURRENT) ==
		  0xFFFFFFFF)
		if (win_errno != NO_ERROR)
		  {
		    win_perror("SetFilePointer()");
		    return -1;
		  }
	    }
	  break;
	}

      // Check for EOF condition
      if (dwReadSize == 0)
	{
	  if (bVerboseMode)
	    fputs("End of input.\r\n", stderr);

	  return 0;
	}

      readbytes.QuadPart += dwReadSize;

      // Check for all-zeroes block
      bool bZeroBlock = false;
      if (bCreateSparse)
	{
	  bZeroBlock = true;
	  for (PULONGLONG bufptr2 = (PULONGLONG) bufptr;
	       bufptr2 < (PULONGLONG) (bufptr + dwReadSize);
	       bufptr2++)
	    if (*bufptr2 != 0)
	      {
		bZeroBlock = false;
		break;
	      }
	}

      DWORD dwWriteSize = 0;
      if (bZeroBlock)
	{
	  LONG lZero = 0;
	  if (SetFilePointer(hOut, dwReadSize, &lZero, FILE_CURRENT) ==
	      0xFFFFFFFF)
	    if (win_errno != NO_ERROR)
	      {
		win_perror("Fatal, cannot move file pointer");
		return -1;
	      }

	  SetEndOfFile(hOut);

	  dwWriteSize = dwReadSize;
	}
      else
	{
	  if (bVerboseMode)
	    fprintf(stderr, "Writing block %I64u\r", writtenblocks);

	  DWORD retries = 0;

	  // Write next block
	  while (!WriteFile(hOut, bufptr, dwReadSize, &dwWriteSize, NULL))
	    {
	      WErrMsg errmsg;

	      if (retries < retrycount)
		{
		  fprintf(stderr, "Rawcopy write failure: %s\nRetrying...\n",
			  (char*)errmsg);
		  retries++;
		  Sleep(500);
		  continue;
		}

	      retries = 0;

	      switch (bIgnoreErrors ? IDIGNORE :
		      MessageBox(NULL, errmsg, "Rawcopy write failure",
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
		      CharToOem(errmsg, errmsg);
		      fprintf(stderr, "Skipping block %I64u: %s\r\n",
			      writtenblocks, errmsg);
		    }

		  LONG lZero = 0;
		  if (SetFilePointer(hOut, dwReadSize - dwWriteSize, &lZero,
				     FILE_CURRENT) == 0xFFFFFFFF)
		    if (win_errno != NO_ERROR)
		      {
			win_perror("Fatal, cannot move file pointer");
			return -1;
		      }
		}
	      break;
	    }
	}

      ++writtenblocks.QuadPart;
      if (copylength.QuadPart != 0)
	if (writtenblocks.QuadPart >= copylength.QuadPart)
	  return 0;

      if (dwWriteSize < dwReadSize)
	fprintf(stderr, "Warning: %u bytes lost.\n",
		dwReadSize - dwWriteSize);
    }
}
