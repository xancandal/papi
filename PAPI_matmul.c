/* 
* My program runs fine when measuring 1 or 2 events, but when I add more I get a -8,
* PAPI_ECNFLCT error code. The error text says, "Event exists. but cannot be counted 
* due to hardware resource limitations". What does this mean?
* You have either exceeded the number of available hardware counters or two or more
* of the events you want to count need the same resources. This can be particularly 
* annoying on machines like the Pentium 4. Although the P4 has 18 nominal counter 
* registers, many events require resources that are restricted to 2 or 3 of these 
* counters. In practice it is often difficult to count more than 4 or 5 simultaneous 
* events on this platform. One way around limited counter resources is to use 
* multiplexing. http://icl.cs.utk.edu/papi/faq/#170 
*/

/* gcc -I/usr/local/include -O0 PAPI_matmul.c /usr/local/lib/libpapi.a -o PAPI_matmul */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <memory.h>
#include <malloc.h>
#include "papi.h"

/* The number of columns of matrix A must be equal to the number of rows of matrix B */

#define mrows 100                      // Number of rows of matrix A (and C)
#define ncolumns 100                   // Number of columns of matrix A (and number of rows of B)
#define pcolumns 100                   // Number of columns of matrix B (and C)

/* Initialize the Matrix arrays function*/
void initmat(double *A, double *B, int m, int n, int p);

/* Matrix-Matrix multiply function */
void matmul(const double *A, const double *B, double *C, int m, int n, int p);

/* Test fail function */
static void test_fail(char *file, int line, char *call, int retval);

int main()
{
    double *a;
    double *b;
    double *c;
    int i = 0, j = 0, k = 0;
    int *events;                        // Array of events
    long long *values;                  // Array of values events
    int EventSet = PAPI_NULL;           // Handle for a PAPI event set as created by PAPI_create_eventset (3) 
    int retval;                         // Test fail function
    int num_event = 0;                  // Number of events
    int max_event;                      // Number of available events
    int EventCode = 0;                  // Event code
    PAPI_event_info_t pset;             // PAPI_event_info_t Struct Reference
    char evname[PAPI_MAX_STR_LEN];      // Symbol event
   
    /* Memory asignament to matrixs*/   
    if((a = (double *)malloc(mrows * ncolumns * sizeof(double))) == NULL)
        printf("Error malloc matrix a[%d]\n",mrows * ncolumns);
    if((b = (double *)malloc(ncolumns * pcolumns * sizeof(double))) == NULL)
        printf("Error malloc matrix b[%d]\n",mrows * ncolumns);
    if((c = (double *)malloc(mrows * pcolumns * sizeof(double))) == NULL)
        printf("Error malloc matrix c[%d]\n",mrows * ncolumns);

    /* Initialize the Matrix arrays */
    initmat(a, b, mrows, ncolumns, pcolumns);

    /* Initialize the PAPI library */
    retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT)
        test_fail( __FILE__, __LINE__, "PAPI_library_init", retval );

    /* Enable and initialize multiplex support */
    retval = PAPI_multiplex_init();
    if ( retval != PAPI_OK )
        test_fail( __FILE__, __LINE__, "PAPI_multiplex_init", retval );
 
    /* Create an EventSet */
    retval = PAPI_create_eventset(&EventSet);
    if ( retval != PAPI_OK )
        test_fail( __FILE__, __LINE__, "PAPI_create_eventset", retval );
 
    /* Assign it to the CPU component */
    retval = PAPI_assign_eventset_component(EventSet, 0);
    if ( retval != PAPI_OK )
        test_fail( __FILE__, __LINE__, "PAPI_assign_eventset_component", retval );
 
    /* Convert the EventSet to a multiplexed event set */
    retval = PAPI_set_multiplex(EventSet);
    if ( retval != PAPI_OK )
        test_fail( __FILE__, __LINE__, "PAPI_set_multiplex", retval );

    /* Obtaining the number of available events */
    max_event = PAPI_get_opt( PAPI_MAX_MPX_CTRS, NULL );
    printf("\nNumber of available events: %d", max_event );
 
    /* Fill up the event set with as many non-derived events as we can */
    EventCode = PAPI_PRESET_MASK;
    do {
        if ( PAPI_get_event_info( EventCode, &pset ) == PAPI_OK ) {
            if ( pset.count && ( strcmp( pset.derived, "NOT_DERIVED" ) == 0 ) ) {
                retval = PAPI_add_event( EventSet, ( int ) pset.event_code );
                if ( retval != PAPI_OK )
                    test_fail( __FILE__, __LINE__, "PAPI_add_event", retval );
                else {
                    //printf( "Added %s\n", pset.symbol );
                    num_event++;
                }
            }
        }
    } while ( ( PAPI_enum_event( &EventCode, PAPI_PRESET_ENUM_AVAIL ) == PAPI_OK ) && ( num_event < max_event ) );
    
    /* Memory asignament to values and events*/    
    events = ( int * ) malloc( ( size_t ) num_event * sizeof ( int ) );
    if ( events == NULL )
        test_fail( __FILE__, __LINE__, "Error malloc events", 0 );
    values = ( long long * ) malloc( ( size_t ) num_event * sizeof ( long long ) );
    if ( values == NULL )
        test_fail( __FILE__, __LINE__, "Erro malloc values", 0 );

    /* Start counting events */
    if ((retval=PAPI_start(EventSet)) != PAPI_OK)
        test_fail(__FILE__, __LINE__, "PAPI_start", retval);

    /* Matrix-Matrix multiply */
    matmul(a, b, c, mrows, ncolumns, pcolumns);

    /* Read the counters */
    if ((retval=PAPI_read( EventSet, values )) != PAPI_OK)
        test_fail(__FILE__, __LINE__, "PAPI_read_counters", retval);
   
    /* Stop counting events */
    if ((retval=PAPI_stop( EventSet, values )) != PAPI_OK)
        test_fail(__FILE__, __LINE__, "PAPI_stop_counters", retval);

    /* List the events in the event set */
    retval = PAPI_list_events( EventSet, events, &num_event );
    if ( retval != PAPI_OK )
        test_fail( __FILE__, __LINE__, "PAPI_list_events", retval );

    /* Print results */
    printf("\nNumber of non-zero events: %d\n", num_event );
    printf( "\nCounts of non-zero available events........................................................\n" );
    printf("Name: \t\t\t  Value: \t Description:\n");
    for ( i = 0; i < num_event; i++ ) {
        PAPI_event_code_to_name( events[i], evname );   // Obtaining name of available events
        PAPI_get_event_info(events[i], &pset);
        if ( values[i] != 0 )  printf("%s \t %15lld \t %s\n", evname, values[i], pset.long_descr);
    }
    printf( "\nCounts of zero available events............................................................\n" );
    printf("Name: \t\t\t  Value: \t Description:\n");
    for ( i = 0; i < num_event; i++ ) {
        PAPI_event_code_to_name( events[i], evname );   // Obtaining name of available events
        PAPI_get_event_info(events[i], &pset);
        if ( values[i] == 0 )  printf("%s \t %15lld \t %s\n", evname, values[i], pset.long_descr);
    }

    /* Check if counter pair(s) had identical values */
    for ( i = 0; i < num_event; i++ ) {
        for ( i = j+1; j < num_event; j++ ) {
            if ( ( i != j ) && ( values[i] == values[j] ) ) k++;  
        }
    }
    if ( k != 0 ) {
        printf( "\nCaution: %d counter pair(s) had identical values\n", k );
    }
    printf("\n");

    /* Free memory */
    free( events );
    free( values );
    free( a );
    free( b );
    free( c );

    /* Cleaning events */
    retval = PAPI_cleanup_eventset( EventSet );
    if ( retval != PAPI_OK )
        test_fail( __FILE__, __LINE__, "PAPI_cleanup_eventset", retval );
    
    /* Destroying events */
    retval = PAPI_destroy_eventset( &EventSet );
    if ( retval != PAPI_OK )
        test_fail( __FILE__, __LINE__, "PAPI_destroy_eventset", retval );

    return 0;
}

/* Initialize the Matrix arrays */
void initmat(double *A, double *B, int m, int n, int p) {
    int i, j;
    for(i = 0; i < m; i++) {
        for(j = 0; j < n; j++) {
            A[i * n + j] = 300 + 10 * i + j;
        }
    }
    for(i = 0; i < n; i++) {
        for(j = 0; j < p; j++) {
            B[i * p + j] = 100 + 10 * i + j;
        }
    }
}

/* Matrix-Matrix multiply function */
/* The number of columns of matrix A must be equal to the number of rows of matrix B */
void matmul(const double *A, const double *B, double *C, int m, int n, int p) {
// m Number of rows of matrix A (and C)
// n Number of columns of matrix A (and number of rows of B)
// p Number of columns of matrix B (and C)
// A[i,j] = A[i*num_columns + j]
    int i, j, k;
    for (i = 0; i < m; ++i)
        for (j = 0; j < p; ++j) {
            double sum = 0;
            for (k = 0; k < n; ++k)
                sum += A[i*n + k] * B[k*p + j];
            C[i*p + j] = sum;
        }
}

/* Test fail function */
static void test_fail(char *file, int line, char *call, int retval) {
    printf("%s\tFAILED\nLine # %d\n", file, line);
    if ( retval == PAPI_ESYS ) {
        char buf[128];
        memset( buf, '\0', sizeof(buf) );
        sprintf(buf, "System error in %s:", call );
        perror(buf);
    }
    else if ( retval > 0 ) {
        printf("Error calculating: %s\n", call );
    }
    else {
        printf("Error in %s: %s\n", call, PAPI_strerror(retval) );
    }
    printf("\n");
    exit(1);
}