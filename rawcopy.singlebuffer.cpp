#include <strstream>
#include "winstrct.h"
#include <winioctl>

#define STDBUFSIZ	512
#define MAXBUFSIZ	2949120

char buffer[STDBUFSIZ];
unsigned long copylength = 0,
	written = 0;
unsigned long bufsiz = STDBUFSIZ;

HANDLE hIn = INVALID_HANDLE_VALUE,
	hOut = INVALID_HANDLE_VALUE;

int main(int argc, char **argv)
{
   bool buffermode = false, lockmode = true, verbosemode = false, displayhelp = false;

	// Nice argument parse loop :)
	while( argv[1] ? argv[1][0] ? (argv[1][0]|0x02) == '/' : false : false )
   {
   	while( (++argv[1])[0] )
      	switch( argv[1][0]|0x20 )
			{
   	   	case 'm':
      	   	buffermode = true;
         	   break;
   	   	case 'l':
      	   	lockmode = false;
         	   break;
   	   	case 'v':
      	   	verbosemode = true;
         	   break;
      	   default:
         		displayhelp = true;
	      }

      --argc;
      ++argv;
   }

	if( displayhelp )
   {
      cout << "Utility to read/write shared objects in Windows," << endl <<
         "i. e. direct access to disk devices." << endl <<
         endl <<
         "Written by Olof Lagerkvist 2000" << endl <<
         "http://here.is/olof" << endl <<
         endl <<
      	"Usage:" << endl <<
      	"RAWCOPY [-l] [-m] [[[[[skipforward] copylength] infile] outfile]" << endl <<
         endl <<
			"Default infile/outfile if none or blank given is" << endl <<
         "standard input/output device." << endl <<
         "-l     Access devices without locking access to them. Use" << endl <<
         "       this switch when you are reading/writing physical" << endl <<
         "       drives and other processes are using the drives." << endl <<
         "       Note! This may destroy your data!" << endl <<
         endl <<
         "-m     Try to buffer more of input file into memory" << endl <<
         "       before writing to output file. It is not possible" << endl <<
         "       to ignore read/write failures if this switch is" << endl <<
         "       used." << endl <<
         endl <<
         "Examples for Windows NT:" << endl <<
         "RAWCOPY diskimage.img \\\\.\\A:" << endl <<
         "  Writes a diskette image file called \"diskimage.img\" to" << endl <<
         "  a physical diskette in drive A:." << endl <<
         "RAWCOPY \\\\.\\PIPE\\MYPIPE \"\"" << endl <<
         "  Reads data from named pipe \"MYPIPE\" on the local" << endl <<
         "  machine and write on standard output." << endl <<
         "RAWCOPY 1 \\\\.\\PhysicalDrive0 parttable" << endl <<
         "  Copies sector 1 (partition table) from first physical" << endl <<
         "  harddrive to file called \"parttable\"." << endl;

      return 0;
   }

   unsigned long skipforward = 0;
	if( argc > 4 )
   {
		skipforward = strtol(argv[1], NULL, 0);

      argv++;
      argc--;
   }

	if( argc > 3 )
   {
		if( (copylength = strtoul(argv[1], NULL, 0)) == 0 )
      {
      	cerr << "Invalid length." << endl;
         return -1;
      }

      argv++;
      argc--;
   }

	if( argc > 2 ? argv[1][0] : false )
   	hIn = CreateFile(argv[1], GENERIC_READ,
	     			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
               OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
   else
   	hIn = hStdIn;

   if( hIn == INVALID_HANDLE_VALUE )
   {
   	win_perror(argc > 1 ? argv[1] : "stdin");
      return -1;
   }

   if( skipforward )
	   if(SetFilePointer( hIn, skipforward, &(*new LONG = 0), FILE_CURRENT)
      	== 0xFFFFFFFF )
	   {
   		win_perror("SetFilePointer()");
      	return -1;
	   }

	char *bufptr;

   // If -m option is used we use big buffer
	if( buffermode )
   {
   	if( (bufptr = (char*)LocalAlloc(LPTR, MAXBUFSIZ)) != NULL )
	     	bufsiz = LocalSize(bufptr);
      else
      {
      	bufptr = buffer;
      	win_perror();
      }

      if( verbosemode )
        	cerr << "Buffering " << bufsiz << " bytes." << endl;
   }
   // -m is not used
   else
   {
   	bufptr = buffer;
      if( verbosemode )
	      cerr << "Sequential scan mode used, max read/write buffer is " <<
         	bufsiz << " bytes." << endl;
   }

	// Jump to next parameter if input file was given
   if( argc > 2 )
   {
   	argv++;
      argc--;
   }

	if( argc > 1 ? argv[1][0] : false )
   	hOut = CreateFile(argv[1], GENERIC_WRITE,
  	   			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
     	         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, hIn);
   else
   	hOut = hStdOut;

   if( hOut == INVALID_HANDLE_VALUE )
   {
   	win_perror(argc > 1 ? argv[1] : "stdout");
      return -1;
   }

   DWORD dwZ;
   win_errno = NO_ERROR;
   DeviceIoControl(hIn, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwZ, NULL);
   switch(win_errno)
   {
      case NO_ERROR:
         if( verbosemode )
            cerr << "Source device locked during data transfer." << endl;

      case ERROR_NOT_SUPPORTED:
      case ERROR_INVALID_FUNCTION:
      case ERROR_INVALID_HANDLE:
      case ERROR_INVALID_PARAMETER:
         break;

      default:
         if( lockmode )
         {
            if( verbosemode )
               cerr << "System Error " << win_errno << '.' << endl;

            win_perror("Cannot lock source device (use -l)");
            return -1;
         }
         else
            win_perror("Warning! Source device not locked");
   }

   win_errno = NO_ERROR;
   DeviceIoControl(hOut, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwZ, NULL);
   switch(win_errno)
   {
      case NO_ERROR:
         if( verbosemode )
            cerr << "Target device locked during data transfer." << endl;

      case ERROR_NOT_SUPPORTED:
      case ERROR_INVALID_FUNCTION:
      case ERROR_INVALID_HANDLE:
      case ERROR_INVALID_PARAMETER:
         break;

      default:
         if( lockmode )
         {
            if( verbosemode )
               cerr << "System Error " << win_errno << '.' << endl;

            win_perror("Cannot lock target device (use -l)");
            return -1;
         }
         else
            win_perror("Warning! Target device not locked");
   }

   while(true)
   {
   	Sleep( 0 );

      if( verbosemode )
         cerr << "Reading block " << written << '\r' << flush;

      // Read next block
	   DWORD dwReadSize;
      while( !ReadFile(hIn, bufptr, bufsiz, &dwReadSize, NULL) )
      {
      	// End of pipe?
      	if( win_errno == ERROR_BROKEN_PIPE )
         	return 0;

      	char *errmsg = win_error;
         strstream msgbuf;
         msgbuf << "Read failed: " << errmsg;
         LocalFree( errmsg );
         switch( MessageBox(NULL, msgbuf.str(), "RawCopy Error",
         	MB_ABORTRETRYIGNORE|MB_ICONEXCLAMATION|MB_DEFBUTTON2|
            MB_TASKMODAL) )
         {
         	case IDABORT:
            	exit( -1 );
            case IDRETRY:
            	continue;
            case IDIGNORE:
            	dwReadSize = bufsiz;
               ZeroMemory( bufptr, bufsiz );
            	if( SetFilePointer(hIn, bufsiz, NULL, FILE_CURRENT) == 0xFFFFFFFF )
               {
               	win_perror("SetFilePointer()");
                  exit(-1);
               }
         }
         break;
      }

      // Check for EOF condition
      if( !dwReadSize )
        	return 0;

      if( verbosemode )
         cerr << "Writing block " << written << '\r' << flush;

      // Write next block
      DWORD dwWriteSize;
      while( !WriteFile(hOut, bufptr, dwReadSize, &dwWriteSize, NULL) )
      {
      	char *errmsg = win_error;
         strstream msgbuf;
         msgbuf << "Write failed: " << errmsg;
         LocalFree( errmsg );
         switch( MessageBox(NULL, msgbuf.str(), "RawCopy Error",
         	MB_ABORTRETRYIGNORE|MB_ICONEXCLAMATION|MB_DEFBUTTON2|
            MB_TASKMODAL) )
         {
         	case IDABORT:
            	exit(-1);
            case IDRETRY:
            	continue;
            case IDIGNORE:
            	if( SetFilePointer(hOut, bufsiz, NULL, FILE_CURRENT) == 0xFFFFFFFF )
               {
               	win_perror( "Can't ignore" );
                  exit(-1);
               }
         }
        	break;
      }

      ++written;
      if( copylength )
      	if( written >= copylength )
      		exit(0);

      if( dwWriteSize < dwReadSize )
      	cerr << "Warning: " << (dwWriteSize - dwReadSize) << " bytes lost." << endl;
	}
}

