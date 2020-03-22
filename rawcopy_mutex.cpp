#include "winstrct.h"

#include <stdio.h>
#include <process.h>

#define BUFSIZ 512

char buffer[BUFSIZ][2];
unsigned long copylength = 0,
	written = 0;

void _USERENTRY asynccopy( HANDLE = INVALID_HANDLE_VALUE );

HANDLE hIn = INVALID_HANDLE_VALUE,
	hOut = INVALID_HANDLE_VALUE,
   hReadMutex = NULL,
   hWriteMutex = NULL;

int main( int argc, char **argv, char **/*envp*/ )
{
	if( argc > 3 )
   {
		if( (copylength = strtol( argv[1], NULL, 0 )) == 0 )
      {
      	cerr << "Invalid length." << endl;
         return -1;
      }

      argv++;
      argc--;
   }

	if( argc > 1 )
   	if( argv[1][0] )
	   	hIn = CreateFile( argv[1], GENERIC_READ,
	      			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
   	            OPEN_EXISTING, 0, NULL );
      else
      	hIn = hStdIn;
   else
   	hIn = hStdIn;

   if( hIn == INVALID_HANDLE_VALUE )
   {
   	win_perror( argc > 1 ? argv[1] : "stdin" );
      return -1;
   }

	if( argc > 2 )
   	if( argv[2][0] )
	   	hOut = CreateFile( argv[2], GENERIC_WRITE,
   	   			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
      	         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, hIn );
      else
      	hOut = hStdOut;
   else
   	hOut = hStdOut;

   if( hOut == INVALID_HANDLE_VALUE )
   {
   	win_perror( argc > 2 ? argv[2] : "stdout" );
      return -1;
   }

   if( (hReadMutex = CreateMutex( NULL, false, NULL )) == NULL )
	   asynccopy();

   if( (hWriteMutex = CreateMutex( NULL, false, NULL )) == NULL )
	   asynccopy();

   HANDLE hSubthread = (HANDLE)_beginthread( asynccopy, 4096, (HANDLE)1 );
   if( hSubthread == INVALID_HANDLE_VALUE )
	   asynccopy();

	asynccopy( 0 );

   WaitForSingleObject( hSubthread, INFINITE );

   return 0;
}

void _USERENTRY asynccopy( HANDLE idx )
{
	char *bufptr = buffer[idx == INVALID_HANDLE_VALUE ? 0 : (int)idx];

   while(true)
   {
   	Sleep( 0 );

		// Make sure other thread is finished before we read next block
      if( WaitForSingleObject( hReadMutex, INFINITE ) == WAIT_FAILED )
  	   {
     		win_perror( "I/O wait read-sync error" );
      	exit( -1 );
  	   }

      // Read next block
	   DWORD dwReadSize;
      while( !ReadFile( hIn, bufptr, BUFSIZ, &dwReadSize, NULL ) )
      {
      	char *errmsg = win_error;
         wsprintf( bufptr, "ReadFile() failed: %s", errmsg );
         LocalFree( errmsg );
         switch( MessageBox( NULL, bufptr, "RawCopy Error", MB_ABORTRETRYIGNORE|
         	MB_ICONEXCLAMATION|MB_DEFBUTTON2|MB_TASKMODAL ) )
         {
         	case IDABORT:
            	exit( -1 );
            case IDRETRY:
            	continue;
            case IDIGNORE:
            	dwReadSize = BUFSIZ;
               ZeroMemory( bufptr, BUFSIZ );
            	if( SetFilePointer( hIn, BUFSIZ, NULL, FILE_CURRENT ) == 0xFFFFFFFF )
               {
               	win_perror( "Can't ignore" );
                  exit( -1 );
               }
         }
         break;
      }

      // Check for EOF condition
      if( !dwReadSize )
      {
	      ReleaseMutex( hReadMutex );

         if( idx == INVALID_HANDLE_VALUE )
         	exit( 0 );

        	return;
      }

		// Make sure other thread is finished before we write next block
      if( WaitForSingleObject( hWriteMutex, INFINITE ) == WAIT_FAILED )
  	   {
     		win_perror( "I/O wait write-sync error" );
     		exit( -1 );
      }

      // Let other thread read next block
      if( !ReleaseMutex( hReadMutex ) )
  	   {
     		win_perror( "I/O release read-sync error" );
      	exit( -1 );
  	   }

      // Write next block
      DWORD dwWriteSize;
      while( !WriteFile( hOut, bufptr, dwReadSize, &dwWriteSize, NULL ) )
      {
      	char *errmsg = win_error;
         wsprintf( bufptr, "WriteFile() failed: %s", errmsg );
         LocalFree( errmsg );
         switch( MessageBox( NULL, bufptr, "RawCopy Error", MB_ABORTRETRYIGNORE|
         	MB_ICONEXCLAMATION|MB_DEFBUTTON2|MB_TASKMODAL ) )
         {
         	case IDABORT:
            	exit( -1 );
            case IDRETRY:
            	continue;
            case IDIGNORE:
            	if( SetFilePointer( hOut, BUFSIZ, NULL, FILE_CURRENT ) == 0xFFFFFFFF )
               {
               	win_perror( "Can't ignore" );
                  exit( -1 );
               }
         }
        	break;
      }

      if( copylength )
      	if( ++written >= copylength )
      		exit( 0 );

      if( dwWriteSize < dwReadSize )
      	cerr << "Warning: " << (dwWriteSize - dwReadSize) << " bytes lost." << endl;

      // Let other thread write next block
      if( !ReleaseMutex( hWriteMutex ) )
     	{
     		win_perror( "I/O release write-sync error" );
      	exit( -1 );
  	   }
	}
}

