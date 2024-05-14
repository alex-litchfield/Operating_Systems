
// CSCI 4210 Operating Systems Project Part 1
// Team RISC-VI
//  Om Anavekar, Alex Litchfield, Chaitanya Talluri

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_PROCESSES 26

// Process struct
typedef struct {
    char id;
    int arrival_time;   
    int num_cpu_bursts;
    int* cpu_bursts;
    int* io_bursts;
} Process;

/**
 * Generates a random number based on an exponential distribution, constrained by an upper bound.
 * Uses 'lambda' for the distribution rate and 'upper_bound' to limit the maximum value of the output.
 * 
 * @param lambda Rate parameter for the exponential distribution.
 * @param upper_bound Maximum value for the generated number.
 * @return A random number from an exponential distribution not exceeding 'upper_bound'.
 */
double next_exp(double lambda, int upper_bound) {
    double exp;
    do {
        double r = drand48();
        exp = -log(r) / lambda;
    } while (exp > upper_bound);
    return exp;
}

/**
 * Generates 'n' processes with random CPU and I/O burst times based on an exponential distribution.
 * Adjusts burst times for the last 'ncpu' processes to model CPU-bound behavior. 
 * Uses 'seed' for consistent random number generation, 'lambda' as the rate parameter for the distribution,
 * and 'upper_bound' to cap burst times. Populates the provided 'processes' array with generated data.
 * 
 * @param n Number of processes to generate.
 * @param ncpu Number of CPU-bound processes.
 * @param seed Seed for random number generation.
 * @param lambda Rate parameter for exponential distribution.
 * @param upper_bound Maximum value for burst times.
 * @param processes Pointer to an array of Process structures to store generated data.
 * @return Number of processes successfully generated.
 */
int generate_processes(int n, int ncpu, long int seed, double lambda, int upper_bound, Process *processes) {
    
    // Seed RNG and initialize process counter
    srand48(seed);
    int num_processes = 0;

    // Iterate through all processes
    for (int i = 0; i < n; i++) {

        // Generate process ID
        processes[num_processes].id = 'A' + num_processes;

        // Generate process arrival time
        double arrival_time = next_exp(lambda, upper_bound);
        processes[num_processes].arrival_time = (int)arrival_time;

        // Generate number of CPU bursts
        int num_cpu_bursts = (int)ceil(drand48() * 64);
        processes[num_processes].num_cpu_bursts = num_cpu_bursts;
        processes[num_processes].cpu_bursts = (int*)malloc(processes[num_processes].num_cpu_bursts * sizeof(int));
        processes[num_processes].io_bursts = (int*)malloc((processes[num_processes].num_cpu_bursts - 1) * sizeof(int));

        // Calculate burst times for each burst
        for (int burst = 0; burst < processes[num_processes].num_cpu_bursts; burst++) {

            // Calculate CPU burst time
            double cpu_burst_time = 0.0;
            cpu_burst_time = ceil(next_exp(lambda, upper_bound));

            // Calculate IO burst time
            double io_burst_time = 0.0;
            if (burst < processes[num_processes].num_cpu_bursts - 1) {
                io_burst_time = ceil(next_exp(lambda, upper_bound)) * 10;
            }

            // For CPU bound processes, multiply CPU burst time by 4
            // and divide IO burst time by 8
            if (ncpu > 0 && num_processes >= n - ncpu) {
                cpu_burst_time = cpu_burst_time * 4;
                io_burst_time = io_burst_time / 8;
            }

            // Assign CPU and IO burst times
            processes[num_processes].cpu_bursts[burst] = (int)cpu_burst_time;
            if (burst < processes[num_processes].num_cpu_bursts - 1) {
                processes[num_processes].io_bursts[burst] = (int)io_burst_time;
            }
        }
        num_processes++;
    }
    return num_processes;
}

/**
 * Prints details of the generated processes including their types, IDs, arrival times, 
 * and CPU and I/O burst times. Identifies the last 'ncpu' processes as CPU-bound.
 * 
 * @param n Total number of processes.
 * @param ncpu Number of CPU-bound processes.
 * @param processes Pointer to an array of Process structures containing the process data to be printed.
 */
void print_processes(int n, int ncpu, Process *processes) {
    printf("<<< PROJECT PART I -- process set (n=%d) with %d CPU-bound process%s >>>\n", n, ncpu, (ncpu == 1) ? "" : "es");

    for (int i = 0; i < n; i++) {
        char* process_type = (i >= n - ncpu) ? "CPU" : "I/O";
        printf("%s-bound process %c: arrival time %dms; %d CPU bursts:\n", process_type, processes[i].id, processes[i].arrival_time, processes[i].num_cpu_bursts);
        for (int j = 0; j < processes[i].num_cpu_bursts; j++) {
            printf("--> CPU burst %dms", processes[i].cpu_bursts[j]);
            if (j < processes[i].num_cpu_bursts - 1) {
                printf(" --> I/O burst %dms\n", processes[i].io_bursts[j]);
            } else {
                printf("\n");
            }
        }
    }
}

/**
 * Frees the dynamically allocated memory for CPU and I/O burst arrays in each process.
 * Should be called after process data is no longer needed to avoid memory leaks.
 * 
 * @param n Total number of processes.
 * @param processes Pointer to an array of Process structures whose memory needs to be freed.
 */
void free_memory(int n, Process *processes) {
    for (int i = 0; i < n; i++) {
        free(processes[i].cpu_bursts);
        free(processes[i].io_bursts);
    }
}

int main(int argc, char *argv[]) {

    // Args number check
    if (argc != 6) {
        fprintf(stderr, "ERROR: Invalid number of arguments\n");
        return EXIT_FAILURE;
    }

    // Convert arguments to desired datatypes
    int n = atoi(argv[1]);
    int ncpu = atoi(argv[2]);
    long int seed = atol(argv[3]);
    double lambda = atof(argv[4]);
    int upper_bound = atoi(argv[5]);

    // Ensure given processes are within bounds
    if (n < 1 || n > MAX_PROCESSES) {
        fprintf(stderr, "ERROR: Invalid value for arg[2] (processes)\n");
        return EXIT_FAILURE;        
    }

    // Ensure given CPU bound processes are within bounds
    if (ncpu < 0 || ncpu > n) {
        fprintf(stderr, "ERROR: Invalid value for arg[3] (CPU bound processes)\n");
        return EXIT_FAILURE;        
    }

    // Ensure lambda is nonzero
    if (lambda <= 0) {
        fprintf(stderr, "ERROR: arg[4] (lambda) must be nonzero\n");
        return EXIT_FAILURE;
    }

    // Ensure upper bound is nonzero
    if (upper_bound <= 0) {
        fprintf(stderr, "ERROR: arg[5] (upper bound) must be nonzero\n");
        return EXIT_FAILURE;        
    }

    Process processes[n];
    int num_processes = generate_processes(n, ncpu, seed, lambda, upper_bound, processes);
    print_processes(num_processes, ncpu, processes);
    free_memory(n, processes);
    return 0;
}