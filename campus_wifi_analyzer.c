/* ============================================================================
   Campus Wi-Fi Traffic Pattern Analyzer
   ----------------------------------------------------------------------------
   Menu-driven C application to record, search, sort, and analyze traffic
   data from campus Wi-Fi access points. Demonstrates structures, pointers,
   dynamic memory allocation, file handling, sorting/searching algorithms,
   and POSIX threads for concurrent traffic collection.

   Build:  gcc -Wall -Wextra -pthread campus_wifi_analyzer.c -o wifi_analyzer
   Run  :  ./wifi_analyzer
   ============================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/* ---------- Constants ---------- */
#define NAME_LEN      50
#define LOC_LEN       50
#define TIME_LEN      20
#define DATA_FILE     "wifi_records.dat"
#define REPORT_FILE   "traffic_report.txt"
#define UNDERUTIL_THRESHOLD_USERS 10     /* below this => underutilized   */
#define INITIAL_CAPACITY 4

/* ---------- Data model ---------- */
typedef struct {
    int  ap_id;                     /* unique access point ID            */
    char name[NAME_LEN];            /* AP name, e.g. "AP-LIB-02"          */
    char location[LOC_LEN];         /* e.g. "Library - 2nd Floor"        */
    int  connected_users;
    float data_usage_mb;            /* total data usage in MB            */
    int  duration_minutes;          /* connection duration               */
    char time_of_access[TIME_LEN];  /* e.g. "09:00-10:00"                 */
} APRecord;

/* ---------- Dynamic array (global, shared) ---------- */
static APRecord *records   = NULL;
static int       count     = 0;      /* number of records in use          */
static int       capacity  = 0;      /* allocated capacity                */

/* Mutex protects `records`, `count`, `capacity` across threads            */
static pthread_mutex_t records_lock = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
   Section: Dynamic memory management
   ============================================================================ */

/* Ensure array has room for at least one more record; grow (double) if not. */
static void ensure_capacity(void) {
    if (count < capacity) return;

    int new_capacity = (capacity == 0) ? INITIAL_CAPACITY : capacity * 2;
    APRecord *tmp = (APRecord *) realloc(records, (size_t) new_capacity * sizeof(APRecord));
    if (tmp == NULL) {
        fprintf(stderr, "Fatal: memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    records  = tmp;
    capacity = new_capacity;
}

/* ============================================================================
   Section: Utility helpers
   ============================================================================ */

static void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* discard */ }
}

static void read_line(char *buf, int max_len) {
    if (fgets(buf, max_len, stdin) != NULL) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    }
}

/* Find the array index of a record by AP id in the CURRENT (unsorted) order.
   Returns -1 if not found. Linear search: O(n).                           */
static int find_index_by_id_unsorted(int id) {
    for (int i = 0; i < count; i++) {
        if (records[i].ap_id == id) return i;
    }
    return -1;
}

/* ============================================================================
   Section: CRUD operations (Create, Modify, Search, Display)
   ============================================================================ */

static void add_record(void) {
    pthread_mutex_lock(&records_lock);
    ensure_capacity();
    APRecord *r = &records[count];

    printf("\n--- Add New Traffic Record ---\n");
    printf("AP ID (integer): ");
    while (scanf("%d", &r->ap_id) != 1) {
        printf("Invalid input. Enter AP ID (integer): ");
        clear_input_buffer();
    }
    clear_input_buffer();

    if (find_index_by_id_unsorted(r->ap_id) != -1) {
        printf("Error: AP ID %d already exists. Use Modify instead.\n", r->ap_id);
        pthread_mutex_unlock(&records_lock);
        return;
    }

    printf("AP Name: ");
    read_line(r->name, NAME_LEN);
    printf("Location: ");
    read_line(r->location, LOC_LEN);

    printf("Connected Users: ");
    while (scanf("%d", &r->connected_users) != 1) {
        printf("Invalid input. Connected Users: ");
        clear_input_buffer();
    }
    clear_input_buffer();

    printf("Data Usage (MB): ");
    while (scanf("%f", &r->data_usage_mb) != 1) {
        printf("Invalid input. Data Usage (MB): ");
        clear_input_buffer();
    }
    clear_input_buffer();

    printf("Connection Duration (minutes): ");
    while (scanf("%d", &r->duration_minutes) != 1) {
        printf("Invalid input. Duration (minutes): ");
        clear_input_buffer();
    }
    clear_input_buffer();

    printf("Time of Access (e.g. 09:00-10:00): ");
    read_line(r->time_of_access, TIME_LEN);

    count++;
    pthread_mutex_unlock(&records_lock);
    printf("Record added successfully. Total records: %d\n", count);
}

static void modify_record(void) {
    int id;
    printf("\n--- Modify Record ---\nEnter AP ID to modify: ");
    while (scanf("%d", &id) != 1) {
        printf("Invalid input. Enter AP ID: ");
        clear_input_buffer();
    }
    clear_input_buffer();

    pthread_mutex_lock(&records_lock);
    int idx = find_index_by_id_unsorted(id);
    if (idx == -1) {
        printf("AP ID %d not found.\n", id);
        pthread_mutex_unlock(&records_lock);
        return;
    }

    APRecord *r = &records[idx];
    printf("Editing AP %d (%s). Press Enter to keep current value.\n", r->ap_id, r->name);

    char buf[NAME_LEN];
    printf("Name [%s]: ", r->name);
    read_line(buf, NAME_LEN);
    if (strlen(buf) > 0) strcpy(r->name, buf);

    printf("Location [%s]: ", r->location);
    read_line(buf, LOC_LEN);
    if (strlen(buf) > 0) strcpy(r->location, buf);

    printf("Connected Users [%d] (enter -1 to keep): ", r->connected_users);
    int iv;
    if (scanf("%d", &iv) == 1 && iv != -1) r->connected_users = iv;
    clear_input_buffer();

    printf("Data Usage (MB) [%.2f] (enter -1 to keep): ", r->data_usage_mb);
    float fv;
    if (scanf("%f", &fv) == 1 && fv != -1.0f) r->data_usage_mb = fv;
    clear_input_buffer();

    printf("Duration (minutes) [%d] (enter -1 to keep): ", r->duration_minutes);
    if (scanf("%d", &iv) == 1 && iv != -1) r->duration_minutes = iv;
    clear_input_buffer();

    printf("Time of Access [%s]: ", r->time_of_access);
    read_line(buf, TIME_LEN);
    if (strlen(buf) > 0) strcpy(r->time_of_access, buf);

    pthread_mutex_unlock(&records_lock);
    printf("Record updated.\n");
}

/* Unsorted search: linear scan by AP ID. */
static void search_unsorted(void) {
    int id;
    printf("\n--- Search (Unsorted / Linear) ---\nEnter AP ID: ");
    while (scanf("%d", &id) != 1) { printf("Invalid input. Enter AP ID: "); clear_input_buffer(); }
    clear_input_buffer();

    pthread_mutex_lock(&records_lock);
    int idx = find_index_by_id_unsorted(id);
    if (idx == -1) {
        printf("AP ID %d not found.\n", id);
    } else {
        APRecord *r = &records[idx];
        printf("Found -> ID:%d Name:%s Location:%s Users:%d Data:%.2fMB Duration:%dmin Time:%s\n",
               r->ap_id, r->name, r->location, r->connected_users,
               r->data_usage_mb, r->duration_minutes, r->time_of_access);
    }
    pthread_mutex_unlock(&records_lock);
}

static int cmp_by_id(const void *a, const void *b) {
    const APRecord *ra = (const APRecord *) a;
    const APRecord *rb = (const APRecord *) b;
    return ra->ap_id - rb->ap_id;
}

/* Sorted search: sort a temporary copy by AP ID, then binary search. O(n log n) + O(log n). */
static void search_sorted_binary(void) {
    if (count == 0) { printf("No records to search.\n"); return; }
    int id;
    printf("\n--- Search (Sorted / Binary) ---\nEnter AP ID: ");
    while (scanf("%d", &id) != 1) { printf("Invalid input. Enter AP ID: "); clear_input_buffer(); }
    clear_input_buffer();

    pthread_mutex_lock(&records_lock);
    APRecord *copy = (APRecord *) malloc((size_t) count * sizeof(APRecord));
    if (!copy) { fprintf(stderr, "Memory allocation failed.\n"); pthread_mutex_unlock(&records_lock); return; }
    memcpy(copy, records, (size_t) count * sizeof(APRecord));
    pthread_mutex_unlock(&records_lock);

    qsort(copy, (size_t) count, sizeof(APRecord), cmp_by_id);

    int lo = 0, hi = count - 1, found = -1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (copy[mid].ap_id == id) { found = mid; break; }
        else if (copy[mid].ap_id < id) lo = mid + 1;
        else hi = mid - 1;
    }

    if (found == -1) {
        printf("AP ID %d not found.\n", id);
    } else {
        APRecord *r = &copy[found];
        printf("Found -> ID:%d Name:%s Location:%s Users:%d Data:%.2fMB Duration:%dmin Time:%s\n",
               r->ap_id, r->name, r->location, r->connected_users,
               r->data_usage_mb, r->duration_minutes, r->time_of_access);
    }
    free(copy);
}

static void display_all(void) {
    pthread_mutex_lock(&records_lock);
    if (count == 0) {
        printf("\nNo traffic records available.\n");
        pthread_mutex_unlock(&records_lock);
        return;
    }
    printf("\n%-5s %-14s %-20s %-7s %-10s %-9s %-12s\n",
           "ID", "Name", "Location", "Users", "Data(MB)", "Dur(min)", "Time");
    printf("--------------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        APRecord *r = &records[i];
        printf("%-5d %-14s %-20s %-7d %-10.2f %-9d %-12s\n",
               r->ap_id, r->name, r->location, r->connected_users,
               r->data_usage_mb, r->duration_minutes, r->time_of_access);
    }
    pthread_mutex_unlock(&records_lock);
}

/* ============================================================================
   Section: Sorting algorithms (rank APs by traffic volume or user count)
   ============================================================================ */

/* Bubble sort by data_usage_mb, descending (highest-traffic AP first).
   Chosen for its simplicity and to explicitly demonstrate pointer-based
   swapping, as required by the assignment brief. O(n^2).                  */
static void bubble_sort_by_data_usage_desc(APRecord *arr, int n) {
    for (int i = 0; i < n - 1; i++) {
        int swapped = 0;
        for (int j = 0; j < n - 1 - i; j++) {
            if (arr[j].data_usage_mb < arr[j + 1].data_usage_mb) {
                APRecord tmp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
                swapped = 1;
            }
        }
        if (!swapped) break;
    }
}

/* Quicksort by connected_users, descending (ranks APs by concurrent load).
   Uses qsort() for an O(n log n) alternative to the bubble sort above.    */
static int cmp_by_users_desc(const void *a, const void *b) {
    const APRecord *ra = (const APRecord *) a;
    const APRecord *rb = (const APRecord *) b;
    return rb->connected_users - ra->connected_users;
}

static void sort_and_rank(void) {
    if (count == 0) { printf("No records to sort.\n"); return; }
    printf("\nRank by: 1) Data usage (bubble sort)  2) Connected users (quicksort)\nChoice: ");
    int choice;
    while (scanf("%d", &choice) != 1) { clear_input_buffer(); printf("Choice: "); }
    clear_input_buffer();

    pthread_mutex_lock(&records_lock);
    APRecord *copy = (APRecord *) malloc((size_t) count * sizeof(APRecord));
    memcpy(copy, records, (size_t) count * sizeof(APRecord));
    pthread_mutex_unlock(&records_lock);

    if (choice == 1) {
        bubble_sort_by_data_usage_desc(copy, count);
        printf("\nRanked by Data Usage (highest first):\n");
    } else {
        qsort(copy, (size_t) count, sizeof(APRecord), cmp_by_users_desc);
        printf("\nRanked by Connected Users (highest first):\n");
    }

    printf("%-5s %-14s %-20s %-7s %-10s\n", "Rank", "Name", "Location", "Users", "Data(MB)");
    for (int i = 0; i < count; i++) {
        printf("%-5d %-14s %-20s %-7d %-10.2f\n",
               i + 1, copy[i].name, copy[i].location, copy[i].connected_users, copy[i].data_usage_mb);
    }
    free(copy);
}

/* ============================================================================
   Section: Statistics and congestion analysis
   ============================================================================ */

static void calculate_statistics(void) {
    pthread_mutex_lock(&records_lock);
    if (count == 0) {
        printf("No records available for analysis.\n");
        pthread_mutex_unlock(&records_lock);
        return;
    }

    long total_users = 0;
    float total_data = 0.0f;
    int peak_idx = 0;   /* highest single-record user count -> proxy for peak period */
    int highest_util_idx = 0;

    for (int i = 0; i < count; i++) {
        total_users += records[i].connected_users;
        total_data  += records[i].data_usage_mb;
        if (records[i].connected_users > records[peak_idx].connected_users) peak_idx = i;
        if (records[i].data_usage_mb > records[highest_util_idx].data_usage_mb) highest_util_idx = i;
    }
    float avg_data = total_data / (float) count;

    printf("\n===== Traffic Statistics =====\n");
    printf("Total records analyzed   : %d\n", count);
    printf("Total connected users    : %ld\n", total_users);
    printf("Average data usage       : %.2f MB\n", avg_data);
    printf("Peak traffic period      : %s (at %s, %d users)\n",
           records[peak_idx].time_of_access, records[peak_idx].name, records[peak_idx].connected_users);
    printf("Highest-utilized AP      : %s at %s (%.2f MB)\n",
           records[highest_util_idx].name, records[highest_util_idx].location,
           records[highest_util_idx].data_usage_mb);

    printf("Underutilized APs (< %d users):\n", UNDERUTIL_THRESHOLD_USERS);
    int found_under = 0;
    for (int i = 0; i < count; i++) {
        if (records[i].connected_users < UNDERUTIL_THRESHOLD_USERS) {
            printf("  - %s at %s (%d users)\n", records[i].name, records[i].location, records[i].connected_users);
            found_under = 1;
        }
    }
    if (!found_under) printf("  (none)\n");
    pthread_mutex_unlock(&records_lock);
}

/* Writes a consolidated report to REPORT_FILE, combining stats + full listing. */
static void generate_report(void) {
    pthread_mutex_lock(&records_lock);
    if (count == 0) {
        printf("No records available to generate a report.\n");
        pthread_mutex_unlock(&records_lock);
        return;
    }

    FILE *fp = fopen(REPORT_FILE, "w");
    if (!fp) { printf("Error: could not create report file.\n"); pthread_mutex_unlock(&records_lock); return; }

    long total_users = 0;
    float total_data = 0.0f;
    int peak_idx = 0, highest_util_idx = 0;
    for (int i = 0; i < count; i++) {
        total_users += records[i].connected_users;
        total_data  += records[i].data_usage_mb;
        if (records[i].connected_users > records[peak_idx].connected_users) peak_idx = i;
        if (records[i].data_usage_mb > records[highest_util_idx].data_usage_mb) highest_util_idx = i;
    }
    float avg_data = total_data / (float) count;

    fprintf(fp, "CAMPUS WI-FI TRAFFIC ANALYSIS REPORT\n");
    fprintf(fp, "=====================================\n\n");
    fprintf(fp, "Total records            : %d\n", count);
    fprintf(fp, "Total connected users     : %ld\n", total_users);
    fprintf(fp, "Average data usage        : %.2f MB\n", avg_data);
    fprintf(fp, "Peak traffic period       : %s (%s, %d users)\n",
            records[peak_idx].time_of_access, records[peak_idx].name, records[peak_idx].connected_users);
    fprintf(fp, "Highest-utilized AP       : %s at %s (%.2f MB)\n\n",
            records[highest_util_idx].name, records[highest_util_idx].location, records[highest_util_idx].data_usage_mb);

    fprintf(fp, "Underutilized APs (< %d users):\n", UNDERUTIL_THRESHOLD_USERS);
    int found_under = 0;
    for (int i = 0; i < count; i++) {
        if (records[i].connected_users < UNDERUTIL_THRESHOLD_USERS) {
            fprintf(fp, "  - %s at %s (%d users)\n", records[i].name, records[i].location, records[i].connected_users);
            found_under = 1;
        }
    }
    if (!found_under) fprintf(fp, "  (none)\n");

    fprintf(fp, "\nFull Record Listing:\n");
    fprintf(fp, "%-5s %-14s %-20s %-7s %-10s %-9s %-12s\n",
            "ID", "Name", "Location", "Users", "Data(MB)", "Dur(min)", "Time");
    for (int i = 0; i < count; i++) {
        APRecord *r = &records[i];
        fprintf(fp, "%-5d %-14s %-20s %-7d %-10.2f %-9d %-12s\n",
                r->ap_id, r->name, r->location, r->connected_users,
                r->data_usage_mb, r->duration_minutes, r->time_of_access);
    }

    fclose(fp);
    pthread_mutex_unlock(&records_lock);
    printf("Report written to %s\n", REPORT_FILE);
}

/* ============================================================================
   Section: File persistence (binary file, multi-session history)
   ============================================================================ */

static void save_to_file(void) {
    pthread_mutex_lock(&records_lock);
    FILE *fp = fopen(DATA_FILE, "wb");
    if (!fp) { printf("Error: could not open %s for writing.\n", DATA_FILE); pthread_mutex_unlock(&records_lock); return; }
    fwrite(&count, sizeof(int), 1, fp);
    fwrite(records, sizeof(APRecord), (size_t) count, fp);
    fclose(fp);
    pthread_mutex_unlock(&records_lock);
    printf("Saved %d record(s) to %s.\n", count, DATA_FILE);
}

static void load_from_file(void) {
    FILE *fp = fopen(DATA_FILE, "rb");
    if (!fp) { printf("No existing data file (%s) found. Starting fresh.\n", DATA_FILE); return; }

    int saved_count;
    if (fread(&saved_count, sizeof(int), 1, fp) != 1) { fclose(fp); return; }

    pthread_mutex_lock(&records_lock);
    capacity = saved_count > 0 ? saved_count : INITIAL_CAPACITY;
    records = (APRecord *) realloc(records, (size_t) capacity * sizeof(APRecord));
    if (!records) { fprintf(stderr, "Memory allocation failed on load.\n"); exit(EXIT_FAILURE); }
    count = (int) fread(records, sizeof(APRecord), (size_t) saved_count, fp);
    pthread_mutex_unlock(&records_lock);

    fclose(fp);
    printf("Loaded %d record(s) from %s.\n", count, DATA_FILE);
}

/* ============================================================================
   Section: System-level extension - concurrent traffic collection (pthreads)
   ----------------------------------------------------------------------------
   Simulates several access points reporting traffic samples concurrently.
   Each worker thread generates one new record and appends it to the shared
   `records` array. The mutex `records_lock` guards every read/modify/write
   of the shared array so concurrent appends cannot corrupt `count`/`capacity`
   or interleave partially-written structs (a classic race condition).
   ============================================================================ */

typedef struct {
    int  ap_id;
    const char *name;
    const char *location;
} ThreadArg;

static void *collect_traffic_worker(void *arg) {
    ThreadArg *ta = (ThreadArg *) arg;

    /* Simulate variable collection latency per AP (I/O wait, network poll). */
    usleep((useconds_t) (10000 + (ta->ap_id * 3000) % 20000));

    APRecord sample;
    sample.ap_id = ta->ap_id;
    strncpy(sample.name, ta->name, NAME_LEN - 1); sample.name[NAME_LEN - 1] = '\0';
    strncpy(sample.location, ta->location, LOC_LEN - 1); sample.location[LOC_LEN - 1] = '\0';
    sample.connected_users  = 5 + (ta->ap_id * 7) % 60;
    sample.data_usage_mb    = (float) (100 + (ta->ap_id * 37) % 900);
    sample.duration_minutes = 15 + (ta->ap_id * 5) % 45;
    strncpy(sample.time_of_access, "AUTO-COLLECTED", TIME_LEN - 1);
    sample.time_of_access[TIME_LEN - 1] = '\0';

    /* Critical section: append to the shared dynamic array. */
    pthread_mutex_lock(&records_lock);
    ensure_capacity();
    /* Overwrite if this AP id already exists (simulated re-poll), else append. */
    int idx = find_index_by_id_unsorted(sample.ap_id);
    if (idx != -1) {
        records[idx] = sample;
    } else {
        records[count++] = sample;
    }
    pthread_mutex_unlock(&records_lock);

    printf("[thread %lu] collected sample for AP %d (%s)\n",
           (unsigned long) pthread_self(), sample.ap_id, sample.name);
    return NULL;
}

static void run_concurrent_collection_demo(void) {
    #define DEMO_AP_COUNT 5
    ThreadArg args[DEMO_AP_COUNT] = {
        {901, "AP-LAB-01", "CS Lab 1"},
        {902, "AP-LIB-01", "Library Ground Floor"},
        {903, "AP-HOS-03", "Hostel Block C"},
        {904, "AP-ADM-01", "Admin Building"},
        {905, "AP-CLS-07", "Classroom 7"},
    };
    pthread_t threads[DEMO_AP_COUNT];

    printf("\nLaunching %d concurrent collection threads...\n", DEMO_AP_COUNT);
    for (int i = 0; i < DEMO_AP_COUNT; i++) {
        if (pthread_create(&threads[i], NULL, collect_traffic_worker, &args[i]) != 0) {
            fprintf(stderr, "Error creating thread %d\n", i);
        }
    }
    for (int i = 0; i < DEMO_AP_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("All collection threads finished. Total records now: %d\n", count);
    #undef DEMO_AP_COUNT
}

/* ============================================================================
   Section: Menu / driver
   ============================================================================ */

static void print_menu(void) {
    printf("\n================ Campus Wi-Fi Traffic Pattern Analyzer ================\n");
    printf(" 1. Add traffic record\n");
    printf(" 2. Modify traffic record\n");
    printf(" 3. Search record (unsorted / linear)\n");
    printf(" 4. Search record (sorted / binary)\n");
    printf(" 5. Display all records\n");
    printf(" 6. Sort & rank access points\n");
    printf(" 7. Calculate statistics\n");
    printf(" 8. Generate consolidated report (%s)\n", REPORT_FILE);
    printf(" 9. Save records to file (%s)\n", DATA_FILE);
    printf("10. Load records from file\n");
    printf("11. Run concurrent traffic collection demo (pthreads)\n");
    printf(" 0. Exit\n");
    printf("=========================================================================\n");
    printf("Enter choice: ");
}

int main(void) {
    load_from_file();   /* attempt to restore history from a previous session */

    int choice;
    do {
        print_menu();
        if (scanf("%d", &choice) != 1) {
            clear_input_buffer();
            printf("Invalid input.\n");
            continue;
        }
        clear_input_buffer();

        switch (choice) {
            case 1:  add_record(); break;
            case 2:  modify_record(); break;
            case 3:  search_unsorted(); break;
            case 4:  search_sorted_binary(); break;
            case 5:  display_all(); break;
            case 6:  sort_and_rank(); break;
            case 7:  calculate_statistics(); break;
            case 8:  generate_report(); break;
            case 9:  save_to_file(); break;
            case 10: load_from_file(); break;
            case 11: run_concurrent_collection_demo(); break;
            case 0:  printf("Saving and exiting...\n"); save_to_file(); break;
            default: printf("Invalid choice. Try again.\n");
        }
    } while (choice != 0);

    free(records);
    pthread_mutex_destroy(&records_lock);
    return 0;
}
