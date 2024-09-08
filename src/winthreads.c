#if 0
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

#define MAX_THREADS 24
#define BUF_SIZE 255

DWORD WINAPI MyThreadFunction( LPVOID lpParam );
void ErrorHandler(LPTSTR lpszFunction);

typedef struct MyData {
    int val1;
    int val2;
} MYDATA, *PMYDATA;

int
main(void)
{
    PMYDATA pDataArray[MAX_THREADS];
    DWORD   dwThreadIdArray[MAX_THREADS];
    HANDLE  hThreadArray[MAX_THREADS]; 

    for (int i = 0; i < MAX_THREADS; i++ )
	{
        pDataArray[i] = (PMYDATA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                sizeof(MYDATA));

        if( pDataArray[i] == NULL )
        {
            ExitProcess(2);
        }

        pDataArray[i]->val1 = i;
        pDataArray[i]->val2 = i+100;

        hThreadArray[i] = CreateThread( 
            NULL,                   // default security attributes
            0,                      // use default stack size  
            MyThreadFunction,       // thread function name
            pDataArray[i],          // argument to thread function 
            0,                      // use default creation flags 
            &dwThreadIdArray[i]);   // returns the thread identifier 

        if (hThreadArray[i] == NULL) 
        {
           ErrorHandler(TEXT("CreateThread"));
           ExitProcess(3);
        }
    }
    WaitForMultipleObjects(MAX_THREADS, hThreadArray, TRUE, INFINITE);
    for(int i=0; i<MAX_THREADS; i++)
    {
        CloseHandle(hThreadArray[i]);
        if(pDataArray[i] != NULL)
        {
            HeapFree(GetProcessHeap(), 0, pDataArray[i]);
            pDataArray[i] = NULL;    // Ensure address is not reused.
        }
    }
    return 0;
}

DWORD WINAPI 
MyThreadFunction(LPVOID lpParam) 
{ 
    HANDLE hStdout;
    PMYDATA pDataArray;

    pDataArray = (PMYDATA)lpParam;
    return 0; 
} 
#endif
