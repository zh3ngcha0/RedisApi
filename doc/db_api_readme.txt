

1.set
setkey  = DBTBLSET_0100000005(DBTBLSET_#db_api_version#dbtbl_id)(generate by construct_setkey_common)
sadd DBTBLSET_0100000005 hashkey1 hashkey2 hashkey3 ......  

2.hash 
hashkey1 = 0100000005hashkey1(db_api_version#dbtbl_id#hashkey1) (generate by construct_hashkey_common)

db_rows_1:hmset 0100000005hashkey1 column_id_1 column_data_1 column_id_2 column_data_2 column_id_3 column_data_3 ......   

db_rows_2:hmset 0100000005hashkey2 column_id_1 column_data_1 column_id_2 column_data_2 column_id_3 column_data_3 ...... 