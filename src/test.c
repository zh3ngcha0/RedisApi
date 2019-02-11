#include <glib.h>
#include <string.h>
#include <unistd.h>
#include "external.h"
#include "record_ha.pb-c.h"
#include "db_api.h"
#include "record_common.h"
#include "scheduler.h"
#include "vnf_timer.h"
#include "db_impl_sync.h"
#include "db_api_sync.h"
#include <stdlib.h>


int key1 = 1236;
db_handle_t * db_handle = NULL;
int perf_op_mode = 0;
unsigned perf_db_id;

extern int testsyncmain(char *server_ip, int port, bool interact);
static void do_perf_test();
static void do_func_test();

void test_write_cb (db_error_t err,
                    void *cb_arg, 
                    uint8_t * key, uint8_t key_len)
{
    printf("in %s Receive\n", __FUNCTION__);

    //db_bind_interface("10.3.180.80");
}

void test_read_cb (db_error_t err,
                   void *cb_arg, 
                   uint8_t *key, uint8_t key_len, 
                   db_column_t *reply_data, size_t reply_data_num)
{
    printf("in %s Receive\n", __FUNCTION__);
}

void test_read_complete_cb (db_error_t err,
                            void *cb_arg)
{
    printf("in %s Receive, start delete in 3 seconds\n", __FUNCTION__);

    sleep(3);

    db_delete_single_row(db_handle, (void*)&key1, sizeof(int));
}

void test_ctrl_cb (void *cb_arg, db_error_t err)
{
    printf("in %s Receive %s\n", __FUNCTION__, db_error_str(err));
}

int file_exist( const char *file )
{
        if ( access(file, F_OK) == 0 )
                return TRUE;
        else
                return FALSE;
}


int main(int argc, char **argv)
{
    int cb_data = 2, ret2=0;
    int port = 6379;
    bool interact = false;

    // run functional test
    if (argc >= 2 && !strcmp(argv[1], "func"))
    {
        do_func_test();
        return 0;
    }

    // run async perf test
    if (argc >= 2 && !strcmp(argv[1], "perf"))
    {
        if (argc != 4)
        {
            printf("cmd format: test perf op_mode[0..1] id[0..29]\n");
            return 0;
        }

        // 0: write; 1: reado
        // should do write first to populate DB
        perf_op_mode = atoi(argv[2]);
        if (perf_op_mode < 0 || perf_op_mode > 1)
        {
            printf("cmd format: test perf_op_mode[0..1] id[0..29]\n");
            return 0;
        }

        // DB id
        perf_db_id = atoi(argv[3]);
        if (perf_db_id < 0 || perf_db_id > 29)
        {
            printf("cmd format: test perf op_mode[0..1] id[0..29]\n");
            return 0;
        }

        do_perf_test();
        return 0;
    }


    // run sync API tests
    if (argc > 1) {
      if (argc > 2) {
        port = atoi(argv[2]);
      }
      if (argc > 3) {
        interact = true;
      }
      testsyncmain(argv[1], port, interact);
      return 0;
    }

    //record_meta_table_init();

    scheduler_init();

    db_open(1, &db_handle, &cb_data, 
                  test_ctrl_cb, test_write_cb, test_read_cb, test_read_complete_cb);
    

    /* test write_single_row */
    db_column_t db_child_columns[2];
    
    HaChildInfo* bkChildInfo = (HaChildInfo*) g_malloc0(sizeof(HaChildInfo));
    if(!bkChildInfo) { 
        printf("Fail to allocate HaChildInfo");
        return 0;
    } 
    ha_child_info__init(bkChildInfo);

    bkChildInfo->has_authkey = 1;
    bkChildInfo->authkey.data = g_malloc0(5);
    memset(bkChildInfo->authkey.data, 0x33, 5);
    bkChildInfo->authkey.len = 5;

    bkChildInfo->key = g_malloc0(sizeof(HaChildKeyEx));
    ha_child_key_ex__init(bkChildInfo->key);
    bkChildInfo->key->dwid = 0x789a;

    db_child_columns[0].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_IB);
    db_child_columns[0].record_type  = RECORDID(HA, CHILD_INFO);
    db_child_columns[0].record       = (ProtobufCMessage*) bkChildInfo;

    db_child_columns[1].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_OB);
    db_child_columns[1].record_type  = RECORDID(HA, CHILD_INFO);
    db_child_columns[1].record       = (ProtobufCMessage*) bkChildInfo;

    //db_write_single_row(db_handle, (void*)&key1, sizeof(int), db_child_columns, 1);


    printf("Gang --- finish writing sleep 20 seconds before updating a column\n");

    sleep(2);
    

db_column_t one_column;
 
   bkChildInfo->authkey.data = g_malloc0(5);
    memset(bkChildInfo->authkey.data, 0x66, 5);

    one_column.column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_IB);
    one_column.record_type  = RECORDID(HA, CHILD_INFO);
    one_column.record       = (ProtobufCMessage*) bkChildInfo;
 
    db_write_single_column(db_handle, (void*)&key1, sizeof(int), &one_column);


    db_read_all(db_handle);


//  printf("Gang --- finish writing sleep 20 seconds before del\n");

//    sleep(20);
    
//db_delete_single_row(db_handle, (void*)&key1, sizeof(int));
 

    ret2 = run_scheduler();

printf("Gang --- run_scheduler returns %d\n", ret2);


//db_close(db_handle);
    scheduler_deinit();

    return 0;
}


#define PERF_MAX_ROWS              40000
#define PERF_MAX_BACKLOG           100
#define PERF_MAX_LOOP              5
#define PERF_WRITE_COLUMNS         5
struct timeval perf_start_time;
struct timeval perf_end_time;
int perf_rows_written = 0;
int perf_rows_write_requested = 0;
int perf_rows_read = 0;
db_column_t child_columns[5];
HaChildInfo* bkChildInfo[5];
int perf_loop;
unsigned perf_key = 0;
VNFTimer perf_write_timer;
VNFTimer perf_read_timer;
unsigned total_mem_size = 0;

static void perf_write_timeout(void *arg)
{
    int ret;

    printf("async write starts\n");
    
    gettimeofday(&perf_start_time, NULL);

    while (perf_rows_write_requested < PERF_MAX_ROWS
            && (perf_rows_write_requested - perf_rows_written) < PERF_MAX_BACKLOG)
    {
        ret = db_write_single_row(db_handle, (void*)&perf_key, sizeof(perf_key), child_columns, PERF_WRITE_COLUMNS);
        if (ret != DB_SUCCESS)
        {
            printf("db_write_single_row failed: error=%s, key=%d\n", db_error_str(ret), perf_key);
            db_close(db_handle);
            exit(1);
        }
        perf_rows_write_requested++;
        perf_key++;
    }
}

static void perf_read_timeout(void *arg)
{
    printf("async read starts\n");
    
    gettimeofday(&perf_start_time, NULL);
    db_read_all(db_handle);
}


void print_throughput(char *op, struct timeval *perf_start_time, struct timeval *perf_end_time, int total)
{
    unsigned num_secs;
    unsigned num_usecs;
    uint64_t throughput;

    num_secs = perf_end_time->tv_sec - perf_start_time->tv_sec; 
    num_usecs = (num_secs * 1000000 + perf_end_time->tv_usec) - perf_start_time->tv_usec;
    throughput = (uint64_t)total*1000000/num_usecs;

    printf("%s: id=%d, lapse=%u msec, rows=%d, rate=%u/sec, row_mem=%u\n", 
             op, perf_db_id, num_usecs/1000, total, (unsigned) throughput, total_mem_size);
}

void perf_test_write_cb (db_error_t err,
                    void *cb_arg, 
                    uint8_t * key, uint8_t key_len)
{
    int i;
    db_error_t ret;

    //printf("%s: error=%s\n", __FUNCTION__, db_error_str(err));
    if (err != DB_SUCCESS)
    {
        printf("%s: error=%s\n", __FUNCTION__, db_error_str(err));
        db_close(db_handle);
        exit(1);
    }

    perf_rows_written++;
    //printf("perf_rows_written = %d\n", perf_rows_written);

    if (perf_rows_written >= PERF_MAX_ROWS)
    {
        perf_loop++;
        if (perf_loop >= PERF_MAX_LOOP)
        {
            gettimeofday(&perf_end_time, NULL);
            print_throughput("write", &perf_start_time, &perf_end_time, PERF_MAX_LOOP * perf_rows_written); 
            db_close(db_handle);
            exit(0);
            return;
        }

        perf_rows_written = 0;
        perf_rows_write_requested = 0;
        perf_key = 0;
    }

    
    while (perf_rows_write_requested < PERF_MAX_ROWS
            && (perf_rows_write_requested - perf_rows_written) < PERF_MAX_BACKLOG)
    {
        ret = db_write_single_row(db_handle, (void*)&perf_key, sizeof(perf_key), child_columns, PERF_WRITE_COLUMNS);
        //printf("db_write_single_row: error=%s, key=%d\n", db_error_str(ret), perf_key);
        if (ret != DB_SUCCESS)
        {
            printf("db_write_single_row failed: error=%s, key=%d\n", db_error_str(ret), perf_key);
            db_close(db_handle);
            exit(1);
        }
        perf_rows_write_requested++;
        perf_key++;
        //printf("row_write_requested = %d\n", perf_rows_write_requested);
    }

    return;
}

void perf_test_read_cb (db_error_t err,
                   void *cb_arg, 
                   uint8_t *key, uint8_t key_len, 
                   db_column_t *reply_data, size_t reply_data_num)
{
    //printf("%s: error=%s\n", __FUNCTION__, db_error_str(err));
    perf_rows_read++;
}

void perf_test_read_complete_cb (db_error_t err,
                            void *cb_arg)
{
    perf_loop++;
    //printf("%s: error=%s, perf_loop=%d\n", __FUNCTION__, db_error_str(err), perf_loop);
    
    if (perf_loop >= PERF_MAX_LOOP)
    {
        gettimeofday(&perf_end_time, NULL);
        print_throughput("read", &perf_start_time, &perf_end_time, perf_rows_read); 
        db_close(db_handle);
        exit(0);
    }

    db_read_all(db_handle);
}

void perf_test_ctrl_cb (void *cb_arg, db_error_t err)
{
    printf("%s: error=%s\n", __FUNCTION__, db_error_str(err));
}

static void do_perf_test()
{
    int i;
    int cb_data = 5;
    db_error_t ret;

    //record_meta_table_init();
    scheduler_init();

    for (i=0; i < 5; i++)
    {
        bkChildInfo[i] = (HaChildInfo*) g_malloc0(sizeof(HaChildInfo));
        if(!bkChildInfo[i]) { 
            printf("Fail to allocate HaChildInfo");
            return;
        } 
        ha_child_info__init(bkChildInfo[i]);
        bkChildInfo[i]->has_authkey = 1;
        bkChildInfo[i]->key = g_malloc0(sizeof(HaChildKeyEx));
        ha_child_key_ex__init(bkChildInfo[i]->key);
        bkChildInfo[i]->key->dwid = 0x789a;

        child_columns[i].record_type  = RECORDID(HA, CHILD_INFO);
        child_columns[i].record       = (ProtobufCMessage*) bkChildInfo[i];
    }
    bkChildInfo[0]->authkey.data = malloc(200);
    bkChildInfo[0]->authkey.len = 200;
    bkChildInfo[1]->authkey.data = malloc(1000);
    bkChildInfo[1]->authkey.len = 1000;
    bkChildInfo[2]->authkey.data = malloc(500);
    bkChildInfo[2]->authkey.len = 500;
    bkChildInfo[3]->authkey.data = malloc(1000);
    bkChildInfo[3]->authkey.len = 1000;
    bkChildInfo[4]->authkey.data = malloc(500);
    bkChildInfo[4]->authkey.len = 500;

    child_columns[0].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_IB);
    child_columns[1].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_OB);
    child_columns[2].column_id    = DBTBLCOLID(HA, SA, CHILD_STAT_IB);
    child_columns[3].column_id    = DBTBLCOLID(HA, SA, CHILD_STAT_OB);
    child_columns[4].column_id    = DBTBLCOLID(HA, SA, IKE_INFO);

    switch (PERF_WRITE_COLUMNS)
    {
        case 5:
            total_mem_size += 500; 
        case 4:
            total_mem_size += 1000; 
        case 3:
            total_mem_size += 500; 
        case 2:
            total_mem_size += 1000; 
        case 1:
            total_mem_size += 200; 
            break;
        default:
            printf("invalid PERF_WRITE_COLUMNS!\n"); 
            exit(1);
            break;
    }

    ret = db_open(perf_db_id, &db_handle, &cb_data, 
                  perf_test_ctrl_cb, perf_test_write_cb, perf_test_read_cb, perf_test_read_complete_cb);
    if (ret != DB_SUCCESS)
    {
        printf("db_open: error=%s\n", db_error_str(ret));
        exit(1);
    }

    if (perf_op_mode >= 1)
    {
        // let timer cb to start reading
        perf_read_timer = vtimer_new_full(1*1000, (VNFTimerCb)perf_read_timeout, 0);
        vtimer_start(perf_read_timer);
    }
    else
    {
        // let timer cb to start writing
        perf_key = 0;
        perf_write_timer = vtimer_new_full(1*1000, (VNFTimerCb)perf_write_timeout, 0);
        vtimer_start(perf_write_timer);
    }

    run_scheduler();

    //db_close(db_handle);
    scheduler_deinit();
    return;
}

#define FUNC_MAX_ROWS       10
#define FUNC_WRITE_COLUMNS  5 
unsigned func_db_id = 5;
db_handle_sync_t *db_handle_sync;
VNFTimer func_timer;
unsigned func_key = 0;
int func_test_step = 0;
int func_rows_written = 0;
int func_rows_read = 0;

void db_read_total_rows(db_error_t err, void *cb_arg, int answer)
{
    int expected_number = (int) cb_arg;

    if (err != DB_SUCCESS)
    {
        printf("FUNC_TEST: %s: error=%s\n", __FUNCTION__, db_error_str(err));
        goto error;
    }

    if (expected_number >= 0)
    {
        printf("FUNC_TEST: db_get_total_row_number reports: total=%d (expected=%d)\n", answer, expected_number);
        if (answer != expected_number)
        {
            printf("FUNC_TEST: the total row number is unexpected!\n");
            goto error;
        }
    }
    else
    {
        printf("FUNC_TEST: db_get_total_row_number reports: total=%d\n", answer);
    }

    return;

error:
    db_close(db_handle);
    db_close_sync(db_handle_sync);
    exit(1);
}

void initialize_column_data(db_column_t *columns, unsigned key)
{
    int i;
    for (i=0; i < FUNC_WRITE_COLUMNS; i++)
    {
        sprintf(((HaChildInfo *) columns[i].record)->authkey.data, "data:%u:%u", key, columns[i].column_id);
    }
}

void create_column_data(unsigned char *data, unsigned column_id, unsigned key)
{
    sprintf(data, "data:%d:%d", key, column_id); 
}

void print_column_data(char *func, db_column_t *columns, unsigned key, int num_columns)
{
    int i;

    printf("%s: num=%d, key=%u: ", func, num_columns, key);
    for (i=0; i < num_columns; i++)
    {
        printf("%d/%s  ", columns[i].column_id, ((HaChildInfo *) columns[i].record)->authkey.data);
    }
    printf("\n");
}

void func_test_write_cb (db_error_t err, void *cb_arg, 
                        uint8_t * key, uint8_t key_len)
{
    int ret;
    int i;

    //printf("%s: error=%s\n", __FUNCTION__, db_error_str(err));
    if (err != DB_SUCCESS)
    {
        printf("FUNC_TEST: %s: error=%s\n", __FUNCTION__, db_error_str(err));
        goto error;
    }

    func_rows_written++;

    return;

error:
        db_close(db_handle);
        db_close_sync(db_handle_sync);
        exit(1);
}


void func_test_read_cb (db_error_t err, void *cb_arg, 
                        uint8_t *key, uint8_t key_len, 
                        db_column_t *columns, size_t num_columns)
{
    int i;

    //printf("%s: error=%s\n", __FUNCTION__, db_error_str(err));
    if (err != DB_SUCCESS)
    {
        printf("FUNC_TEST: %s: error=%s\n", __FUNCTION__, db_error_str(err));
        goto error;
    }
    
    for (i=0; i < num_columns; i++)
    {
        unsigned char data_buf[128];

        create_column_data(data_buf, columns[i].column_id, *((unsigned *)key));
        if (strcmp(data_buf, ((HaChildInfo *) columns[i].record)->authkey.data))
        {
            printf("FUNC_TEST: %s: data read from db_read_single_row was incorrect: key=%u, cid=%u\n",
                    __FUNCTION__, key, columns[i].column_id);

            goto error;
        }
    }
    print_column_data("FUNC_TEST: func_test_read_cb", columns, *((unsigned *)key), num_columns); 

    func_rows_read++;

    return;

error:
        db_close(db_handle);
        db_close_sync(db_handle_sync);
        exit(1);
}

void func_test_read_complete_cb (db_error_t err, void *cb_arg)
{
    //printf("FUNC_TEST: %s: error=%s\n", __FUNCTION__, db_error_str(err));
    if (err != DB_SUCCESS)
    {
        printf("FUNC_TEST: %s: error=%s\n", __FUNCTION__, db_error_str(err));
        goto error;
    }

    if (func_rows_read != FUNC_MAX_ROWS)
    {
        printf("FUNC_TEST: %s reports only %d out of %d rows were written\n", __FUNCTION__, func_rows_read, FUNC_MAX_ROWS);
        goto error;
    }

    printf("FUNC_TEST: %s reports %d rows were written with expected data\n", __FUNCTION__, func_rows_read);

    return;

error:
        db_close(db_handle);
        db_close_sync(db_handle_sync);
        exit(1);
}

void func_test_ctrl_cb (void *cb_arg, db_error_t err)
{
    printf("FUNC_TEST: %s: error=%s\n", __FUNCTION__, db_error_str(err));
}

static void func_timeout(void *arg)
{
    int ret;
    db_column_t columns[FUNC_WRITE_COLUMNS];
    int i;
    unsigned key;
    unsigned key_len;
    unsigned char key_buf[128];
    unsigned char data_buf[128];
        

    //printf("FUNC_TEST: %s: func_test_step=%d\n", __FUNCTION__, func_test_step);

    switch (func_test_step)
    {
        case 0:
            printf("FUNC_TEST: ASYNC writes %d rows with keys from 0 to %d\n", FUNC_MAX_ROWS, FUNC_MAX_ROWS-1);
            func_key = 0;
            func_rows_written = 0;
            for (i=0; i < FUNC_MAX_ROWS; i++)
            {
                initialize_column_data(child_columns, func_key);
                ret = db_write_single_row(db_handle, (void*)&func_key, sizeof(func_key), child_columns, FUNC_WRITE_COLUMNS);
                if (ret != DB_SUCCESS)
                {
                    printf("FUNC_TEST: db_write_single_row failed: error=%s, key=%d\n", db_error_str(ret), func_key);
                    goto error;
                }
                func_key++;
            }
            break;

        case 1:
            if (func_rows_written != FUNC_MAX_ROWS)
            {
                printf("FUNC_TEST: only %d out of %d rows were written.\n", func_rows_written, FUNC_MAX_ROWS);
                goto error;
            }
            printf("FUNC_TEST: %d writes were confirmed through write_cb\n", FUNC_MAX_ROWS);

            printf("FUNC_TEST: try to verify the written data of written rows with db_get_next_row_sync\n");
            func_rows_written = 0;
            while (1)
            {
                ret = db_get_next_row_sync(db_handle_sync, key_buf, sizeof(key_buf), &key_len);
                if (ret != DB_SUCCESS)
                {
                    printf("FUNC_TEST: %s: error=%s\n", "db_get_next_row_sync", db_error_str(ret));
                    goto error;
                }
        
                if (key_len <= 0) 
                    break;
        
                key = *((unsigned *) key_buf);
                if (key >= FUNC_MAX_ROWS)
                {
                    printf("FUNC_TEST: %s received invalid key = %u\n", "db_get_next_row_sync", key);
                    goto error;
                }
                func_rows_written++;
        
                ret = db_read_single_row_sync(db_handle_sync, &key, sizeof(key), columns, FUNC_WRITE_COLUMNS);
                if (ret != DB_SUCCESS)
                {
                    printf("FUNC_TEST: %s: error=%s\n", "db_read_single_row_sync", db_error_str(ret));
                    goto error;
                }
        
                for (i=0; i< FUNC_WRITE_COLUMNS; i++)
                {
                    create_column_data(data_buf, columns[i].column_id, key);
                    if (strcmp(data_buf, ((HaChildInfo *) columns[i].record)->authkey.data))
                    {
                        printf("FUNC_TEST: data read from db_read_single_row_sync was incorrect: key=%u, cid=%u\n",
                                key, columns[i].column_id);
                        goto error;
                    }
                }
                print_column_data("FUNC_TEST: db_read_single_row_sync", columns, key, FUNC_WRITE_COLUMNS); 
            }
        
            if (func_rows_written != FUNC_MAX_ROWS)
            {
                printf("FUNC_TEST: %s reports only %d out of % rows were written\n", "db_get_next_row_sync", func_rows_written, FUNC_MAX_ROWS);
                goto error;
            }
        
            printf("FUNC_TEST: %s reports all %d rows were written with expected data\n", "db_get_next_row_sync", func_rows_written);
            func_rows_written = 0;

            printf("FUNC_TEST: try to use db_get_total_row_number to verify the number of written rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, (void *) FUNC_MAX_ROWS);
        
            break;

        case 2:
            printf("FUNC_TEST: try to verify the written data of written rows with db_read_all\n");
            func_rows_read = 0;
            db_read_all(db_handle);
            break;
        
        case 3:
            if (func_rows_read != FUNC_MAX_ROWS)
            {
                printf("FUNC_TEST: db_read_all reports only %d out of %d rows were written\n", func_rows_read, FUNC_MAX_ROWS);
                goto error;
            }

            key = FUNC_MAX_ROWS-1;
            memset(columns, 0, FUNC_WRITE_COLUMNS * sizeof(db_column_t));
            printf("FUNC_TEST: try to call db_read_single_row_columns with key=%u, cids: %d %d %d\n",
                    key, 
                    child_columns[0].column_id, 
                    child_columns[2].column_id, 
                    child_columns[4].column_id);
            columns[0].column_id = child_columns[0].column_id;
            columns[1].column_id = child_columns[2].column_id;
            columns[2].column_id = child_columns[4].column_id;
            ret = db_read_single_row_columns(db_handle, &key, sizeof(key), columns, 3);
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_read_single_row_columns failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }

            break;

        case 4:
            key = FUNC_MAX_ROWS;
            initialize_column_data(child_columns, key);

            printf("FUNC_TEST: try to call db_write_single_column with (new) key=%u, cid: %d\n",
                    key, child_columns[0].column_id); 

            ret = db_write_single_column(db_handle, &key, sizeof(key), &child_columns[0]);
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_write_single_column failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }

            break;

        case 5:
            printf("FUNC_TEST: try to read the new row back with db_read_single_row\n");
            key = FUNC_MAX_ROWS;
            ret = db_read_single_row(db_handle, &key, sizeof(key));
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_read_single_row failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }

            break;

        case 6:
            printf("FUNC_TEST: try to use db_get_total_row_number to see current number of rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, (void *) (FUNC_MAX_ROWS+1));

            break;

        case 7:
            printf("FUNC_TEST: try to delete the new row with db_delete_single_row\n");
            key = FUNC_MAX_ROWS;
            ret = db_delete_single_row(db_handle, &key, sizeof(key));
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_delete_single_row failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }

            break;

        case 8:
            printf("FUNC_TEST: try to use db_get_total_row_number to see current number of rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, (void *) FUNC_MAX_ROWS);
            
            break;

        case 9:
            printf("FUNC_TEST: try to delete all rows with db_delete_all_row\n");
            ret = db_delete_all_row(db_handle);
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_delete_all_row failed: error=%s\n", db_error_str(ret));
                goto error;
            }

            break;
        case 10:
            printf("FUNC_TEST: try to use db_get_total_row_number to see current number of rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, 0);

            break;
            
        case 11:
            printf("\n\n\n\n");
            printf("FUNC_TEST: SYNC writes %d rows with keys from 0 to %d\n", FUNC_MAX_ROWS, FUNC_MAX_ROWS-1);
            func_key = 0;
            func_rows_written = 0;
            for (i=0; i < FUNC_MAX_ROWS; i++)
            {
                initialize_column_data(child_columns, func_key);
                ret = db_write_single_row_sync(db_handle_sync, (void*)&func_key, sizeof(func_key), child_columns, FUNC_WRITE_COLUMNS);
                if (ret != DB_SUCCESS)
                {
                    printf("FUNC_TEST: db_write_single_row_sync failed: error=%s, key=%d\n", db_error_str(ret), func_key);
                    goto error;
                }
                func_key++;
                func_rows_written++;
            }

            if (func_rows_written != FUNC_MAX_ROWS)
            {
                printf("FUNC_TEST: only %d out of %d rows were written.\n", func_rows_written, FUNC_MAX_ROWS);
                goto error;
            }
            printf("FUNC_TEST: %d writes were confirmed through db_write_single_row_sync results.\n", func_rows_written);

            printf("FUNC_TEST: try to verify all the written data "
                    "with db_get_next_row_sync and db_read_single_row_sync\n");

            func_rows_written = 0;
            while (1)
            {
                ret = db_get_next_row_sync(db_handle_sync, key_buf, sizeof(key_buf), &key_len);
                if (ret != DB_SUCCESS)
                {
                    printf("FUNC_TEST: %s: error=%s\n", "db_get_next_row_sync", db_error_str(ret));
                    goto error;
                }
        
                if (key_len <= 0) 
                    break;
        
                key = *((unsigned *) key_buf);
                if (key >= FUNC_MAX_ROWS)
                {
                    printf("FUNC_TEST: %s received invalid key = %u\n", "db_get_next_row_sync", key);
                    goto error;
                }
                func_rows_written++;
        
                ret = db_read_single_row_sync(db_handle_sync, &key, sizeof(key), columns, FUNC_WRITE_COLUMNS);
                if (ret != DB_SUCCESS)
                {
                    printf("FUNC_TEST: %s: error=%s\n", "db_read_single_row_sync", db_error_str(ret));
                    goto error;
                }
        
                for (i=0; i< FUNC_WRITE_COLUMNS; i++)
                {
                    create_column_data(data_buf, columns[i].column_id, key);
                    if (strcmp(data_buf, ((HaChildInfo *) columns[i].record)->authkey.data))
                    {
                        printf("FUNC_TEST: data read from db_read_single_row_sync was incorrect: key=%u, cid=%u\n",
                                key, columns[i].column_id);
                        goto error;
                    }
                }
                print_column_data("FUNC_TEST: db_read_single_row_sync", columns, key, FUNC_WRITE_COLUMNS); 
            }
        
            if (func_rows_written != FUNC_MAX_ROWS)
            {
                printf("FUNC_TEST: %s reports only %d out of % rows were written\n", "db_get_next_row_sync", func_rows_written, FUNC_MAX_ROWS);
                goto error;
            }
        
            printf("FUNC_TEST: %s reports all %d rows were written with expected data\n", "db_get_next_row_sync", func_rows_written);
            func_rows_written = 0;

            break;

        case 12:
            printf("FUNC_TEST: try to use db_get_total_row_number to verify the number of written rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, (void *) FUNC_MAX_ROWS);
        
            break;

        case 13:
            printf("FUNC_TEST: try to verify the written data of written rows with db_read_all\n");
            func_rows_read = 0;
            db_read_all(db_handle);

            break;
        
        case 14:
            if (func_rows_read != FUNC_MAX_ROWS)
            {
                printf("FUNC_TEST: db_read_all reports only %d out of %d rows were written\n", func_rows_read, FUNC_MAX_ROWS);
                goto error;
            }

            key = FUNC_MAX_ROWS-1;
            memset(columns, 0, FUNC_WRITE_COLUMNS * sizeof(db_column_t));

            printf("FUNC_TEST: try to call db_read_single_row_columns_sync with key=%u, cids: %d %d %d\n",
                    key, 
                    child_columns[0].column_id, 
                    child_columns[2].column_id, 
                    child_columns[4].column_id);

            columns[0].column_id = child_columns[0].column_id;
            columns[1].column_id = child_columns[2].column_id;
            columns[2].column_id = child_columns[4].column_id;
            ret = db_read_single_row_columns_sync(db_handle_sync, &key, sizeof(key), columns, 3);
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_read_single_row_columns_sync failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }

            print_column_data("FUNC_TEST: db_read_single_row_columns_sync", columns, key, 3); 

            key = FUNC_MAX_ROWS;
            initialize_column_data(child_columns, key);

            printf("FUNC_TEST: try to call db_write_single_column_sync with (new) key=%u, cid: %d\n",
                    key, child_columns[0].column_id); 

            ret = db_write_single_column_sync(db_handle_sync, (void *) &key, sizeof(key), &child_columns[0]);
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_write_single_column_sync failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }

            printf("FUNC_TEST: try to use db_get_total_row_number to see current number of rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, (void *) (FUNC_MAX_ROWS + 1));

            break;

        case 15:
            printf("FUNC_TEST: try to read the new row back with db_read_single_row_sync\n");
            key = FUNC_MAX_ROWS;
            ret = db_read_single_row_sync(db_handle_sync, &key, sizeof(key), columns, FUNC_WRITE_COLUMNS);
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_read_single_row_sync failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }
            print_column_data("FUNC_TEST: db_read_single_row_sync", columns, key, 1); 

            break;

        case 16:
            printf("FUNC_TEST: try to delete the new row with db_delete_single_row_sync\n");
            key = FUNC_MAX_ROWS;
            ret = db_delete_single_row_sync(db_handle_sync, &key, sizeof(key));
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_delete_single_row_sync failed: error=%s, key=%d\n", db_error_str(ret), key);
                goto error;
            }

            printf("FUNC_TEST: try to use db_get_total_row_number to see current number of rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, (void *) FUNC_MAX_ROWS);
            
            break;

        case 17:
            printf("FUNC_TEST: try to delete all rows with db_delete_all_row_sync\n");
            ret = db_delete_all_row_sync(db_handle_sync);
            if (ret != DB_SUCCESS)
            {
                printf("FUNC_TEST: db_delete_all_row_sync failed: error=%s\n", db_error_str(ret));
                goto error;
            }

            printf("FUNC_TEST: try to use db_get_total_row_number to see current number of rows\n");
            db_get_total_row_number(db_handle, db_read_total_rows, 0);

            break;
            
        default:
            printf("FUNC_TEST: test passed - close all db handles\n");
            goto error;
            break;
    }

    func_test_step++;
    vtimer_start(func_timer);

    return;

error:
    db_close(db_handle);
    db_close_sync(db_handle_sync);
    exit(0);
}

static void do_func_test()
{
    int i;
    int cb_data = 0;
    db_error_t ret;
    db_handle_sync_param_t param;

    //record_meta_table_init();
    scheduler_init();

    child_columns[0].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_IB);
    child_columns[1].column_id    = DBTBLCOLID(HA, SA, CHILD_INFO_OB);
    child_columns[2].column_id    = DBTBLCOLID(HA, SA, CHILD_STAT_IB);
    child_columns[3].column_id    = DBTBLCOLID(HA, SA, CHILD_STAT_OB);
    child_columns[4].column_id    = DBTBLCOLID(HA, SA, IKE_INFO);

    for (i=0; i < FUNC_WRITE_COLUMNS; i++)
    {
        bkChildInfo[i] = (HaChildInfo*) g_malloc0(sizeof(HaChildInfo));
        if(!bkChildInfo[i]) { 
            printf("Fail to allocate HaChildInfo");
            return;
        } 
        ha_child_info__init(bkChildInfo[i]);
        bkChildInfo[i]->has_authkey = 1;
        bkChildInfo[i]->key = g_malloc0(sizeof(HaChildKeyEx));
        ha_child_key_ex__init(bkChildInfo[i]->key);
        bkChildInfo[i]->key->dwid = 0x789a;

        child_columns[i].record_type  = RECORDID(HA, CHILD_INFO);
        child_columns[i].record       = (ProtobufCMessage*) bkChildInfo[i];
    }

    printf("******************************************************************\n");
    printf("FUNC_TEST: each row has %d columns with cids:", FUNC_WRITE_COLUMNS);
    for (i=0; i < FUNC_WRITE_COLUMNS; i++)
    {
        bkChildInfo[i]->authkey.data = malloc(50);
        bkChildInfo[i]->authkey.len = 50;
        printf(" %d", child_columns[i].column_id);
    }
    printf("\nFUNC_TEST: each column stores data: \"data:key:cid\"\n");
    printf("FUNC_TEST: Make sure the DB is empty before running this test!\n");
    printf("******************************************************************\n");

    printf("FUNC_TEST: get async db_handle for id=%u\n", func_db_id);
    ret = db_open(func_db_id, &db_handle, &cb_data, 
                  func_test_ctrl_cb, func_test_write_cb, func_test_read_cb, func_test_read_complete_cb);
    if (ret != DB_SUCCESS)
    {
        printf("db_open: error=%s\n", db_error_str(ret));
        exit(1);
    }

    printf("FUNC_TEST: get sync db_handle for id=%u\n", func_db_id);
    memset(&param, 0, sizeof(param));
    ret = db_open_sync(func_db_id, &param, &db_handle_sync);
    if (ret != DB_SUCCESS)
    {
        db_close(db_handle);
        printf("db_open_sync: error=%s\n", db_error_str(ret));
        exit(1);
    }

    func_timer = vtimer_new_full(1*1000, (VNFTimerCb)func_timeout, 0 );
    vtimer_start(func_timer);

    run_scheduler();

    scheduler_deinit();
    return;
}
