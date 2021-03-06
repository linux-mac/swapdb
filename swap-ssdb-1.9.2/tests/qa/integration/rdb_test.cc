#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sstream>
#include "SSDB_client.h"
#include "gtest/gtest.h"
#include "ssdb_test.h"
#include <algorithm>
using namespace std;

class RDBTest : public SSDBTest
{
    public:
        ssdb::Status s;
        static const int BIG_KEY_NUM = 10000;
        std::vector<std::string> list,getList;
        std::vector<std::string> keys;
        std::map<std::string, double> items;
        std::map<std::string, std::string> kvs;
        string key, val, getVal, field, dumpVal, restoreVal;
        uint32_t keysNum;
        double score, getScore;
        int64_t ret, ttl, counts;


        static void* restore_thread_func(void *arg);
        static const int threadsNum = 9;
        pthread_t bg_tid[threadsNum];
        string restorekey;
};

void* RDBTest::restore_thread_func(void *arg) {
    ssdb::Status s_restore;
    RDBTest* mthreadsTest = (RDBTest*)arg;
    ssdb::Client *tmpclient = ssdb::Client::connect(mthreadsTest->ip.data(), mthreadsTest->port);
    string restoreVal;
    if(tmpclient == NULL) {
        cout<<"fail to connect to server in read_thread_func!";
        return (void *)NULL;
    }
    s_restore = tmpclient->restore(mthreadsTest->restorekey, 0, mthreadsTest->dumpVal, "replace", &restoreVal);
    if(!s_restore.ok()) {
        cout<<"restore key fail!"<<mthreadsTest->dumpVal.size()<<s_restore.code()<<endl;
    } else {
        cout<<"success!"<<endl;
    }

    delete tmpclient;
    tmpclient = NULL;
    return (void *)NULL;
}

TEST_F(RDBTest, Test_rdb_base_dump_restore) {

    counts = rand()%9+1;
    key = "key";//GetRandomKey_(); 
    field = "field";//GetRandomField_();
    val = "val";//GetRandomVal_(); 
    score = GetRandomDouble_();
    string Replace = "";
    ASSERT_TRUE(Replace.empty());
    ttl = 0;

    //kv type
    client->del(key);
    client->set(key,val);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"string type dump error."<<endl;

    client->del(key);
    s = client->get(key, &getVal);
    EXPECT_EQ("not_found",s.code())<<"key should be deleted"<<endl;
    s = client->restore(key, ttl, dumpVal, Replace, &restoreVal);
    EXPECT_EQ("ok",s.code())<<"string type restore error."<<endl;
    s = client->get(key, &getVal);
    ASSERT_EQ(val, getVal)<<"string type restore wrong val!"<<endl;

    client->del(key);

    //hash
    kvs.clear();
    keys.clear();
    list.clear();
    for(int n = 0;n < counts; n++){
        kvs.insert(std::make_pair(field+itoa(n), val+itoa(n)));
        keys.push_back(field+itoa(n));
    }

    s = client->multi_hset(key, kvs);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"hash type dump error."<<endl;
    client->del(key);

    s = client->restore(key, ttl, dumpVal, Replace, &restoreVal);
    EXPECT_EQ("ok",s.code())<<"hash type restore error."<<endl;

    s = client->hsize(key, &ret);
    ASSERT_EQ(counts, ret)<<"fail to hsize key!"<<endl;
    s = client->multi_hget(key, keys, &list);
    for(int n = 0;n < counts; n++){
        ASSERT_EQ(field+itoa(n), list[0+2*n]);
        ASSERT_EQ(val+itoa(n), list[1+2*n]);
    }
    client->del(key);

    //list
    s = client->qpush_front(key, keys, &ret);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"list type dump error."<<endl;
    client->del(key);

    s = client->restore(key, ttl, dumpVal, Replace, &restoreVal);
    EXPECT_EQ("ok",s.code())<<"list type restore error."<<endl;

    s = client->qsize(key, &ret);
    ASSERT_EQ(counts, ret)<<"fail to qsize key!"<<endl;

    list.clear();
    s = client->qslice(key, 0, -1, &list);
    sort(list.begin(), list.end());
    for(std::vector<string>::iterator kit = keys.begin(), lit = list.begin();lit != list.end();kit++,lit++)
    {
        ASSERT_EQ(*kit, *lit)<<"list type restore val error."<<endl;

    }
    client->del(key);

    //set
    s = client->sadd(key, keys, &ret);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"set type dump error."<<endl;
    client->del(key);

    s = client->restore(key, ttl, dumpVal, Replace, &restoreVal);
    EXPECT_EQ("ok",s.code())<<"set type restore error."<<endl;

    s = client->scard(key, &ret);
    ASSERT_EQ(counts, ret)<<"fail to scard key!"<<endl;
    list.clear();
    s = client->smembers(key, &list);
    for(std::vector<string>::iterator kit = keys.begin(), lit = list.begin();lit != list.end();kit++,lit++)
    {
        ASSERT_EQ(*kit, *lit)<<"set type restore val error."<<endl;

    }
    client->del(key);

    //zset
    items.clear();
    keys.clear();
    for(int n = 0;n < counts; n++){
        items.insert(std::make_pair(field+itoa(n),score+n));
        keys.push_back(field+itoa(n));
    }
    s = client->zset(key, items, &ret);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"zset type dump error."<<endl;
    client->del(key);

    s = client->restore(key, ttl, dumpVal, Replace, &restoreVal);
    EXPECT_EQ("ok",s.code())<<"zset type restore error."<<endl;
    s = client->zsize(key, &ret);
    ASSERT_EQ(counts, ret)<<"fail to zsize key"<<endl;

    for(int n = 0;n < counts; n++){
        s = client->zget(key, field+itoa(n), &getScore);
        ASSERT_EQ("ok", s.code());
        ASSERT_NEAR(score+n, getScore, eps)<<"zset type restore score error"<<endl;
    }
    client->del(key);
}

TEST_F(RDBTest, Test_rdb_replace_dump_restore) {

    key = GetRandomKey_(); 
    val = GetRandomVal_(); 

    ttl = 0;
    string Replace = "replace";

    client->del(key);
    client->set(key,val);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"string type dump error."<<endl;
    client->set(key,val+"exist");

    s = client->restore(key, ttl, dumpVal, "", &restoreVal);
    EXPECT_EQ("error",s.code())<<"string type restore exist key without replace should be error."<<endl;
    s = client->get(key, &getVal);
    ASSERT_EQ(val+"exist", getVal)<<"string type restore exist key without replace should remain!"<<endl;

    s = client->restore(key, ttl, dumpVal, Replace, &restoreVal);
    EXPECT_EQ("ok",s.code())<<"string type exist key with replace restore error."<<endl;
    s = client->get(key, &getVal);
    ASSERT_EQ(val, getVal)<<"string type exist key with replace restore wrong val!"<<endl;
    client->del(key);
}

#ifdef EXPIRE
TEST_F(RDBTest, Test_rdb_ttl_dump_restore) {

    key = "key";//GetRandomKey_(); 
    val = "val";//GetRandomVal_(); 

    ttl = -1;

    client->del(key);
    client->set(key,val);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"string type dump error."<<endl;

    client->del(key);
    s = client->restore(key, ttl, dumpVal, "", &restoreVal);
    EXPECT_EQ("error",s.code())<<"string type with ttl<0 should restore error."<<endl;
    getVal.clear();
    s = client->get(key, &getVal);
    ASSERT_TRUE(getVal.empty()&&s.not_found())<<"val should null! ";

    ttl = 5000;
    s = client->restore(key, ttl, dumpVal, "", &restoreVal);
    EXPECT_EQ("ok",s.code())<<"string type with ttl>0 restore error."<<endl;
    s = client->get(key, &getVal);
    ASSERT_EQ(val, getVal)<<"string type with ttl>0 restore wrong val!"<<endl;

    s = client->pttl(key, &ret);
    EXPECT_TRUE(ret >= 3000 && ret <= 5000)<<ret<<" ttl should near "<<ttl<<endl;

    client->del(key);
    ttl = 2569591501;
    s = client->restore(key, ttl, dumpVal, "", &restoreVal);
    EXPECT_EQ("ok",s.code())<<"string type with ttl overflow 32 bit integer restore error."<<endl;
    s = client->get(key, &getVal);
    ASSERT_EQ(val, getVal)<<"string type with ttl overflow 32 bit integer restore wrong val!"<<endl;

    s = client->pttl(key, &ret);
    EXPECT_TRUE(ret >= 2569591501-3000 && ret <= 2569591501)<<ret<<" ttl should near "<<ttl<<endl;

    client->del(key);
}
#endif

TEST_F(RDBTest, Test_rdb_syntax_dump_restore) {

    key = GetRandomKey_(); 
    val = GetRandomVal_(); 

    ttl = 0;

    client->del(key);
    client->set(key,val);
    dumpVal.clear();
    s = client->dump(key, &dumpVal);
    EXPECT_EQ("ok",s.code())<<"string type dump error."<<endl;

    client->del(key);
    s = client->restore(key, ttl, dumpVal, "invaild", &restoreVal);
    EXPECT_EQ("error",s.code())<<"with invalid option should restore error."<<endl;
    getVal.clear();
    s = client->get(key, &getVal);
    ASSERT_TRUE(getVal.empty()&&s.not_found())<<"val should null! ";

    s = client->restore(key, ttl, dumpVal+'0', "", &restoreVal);
    EXPECT_EQ("error",s.code())<<"with invalid serialized-value  should restore error."<<endl;
    getVal.clear();
    s = client->get(key, &getVal);
    ASSERT_TRUE(getVal.empty()&&s.not_found())<<"val should null! ";
    client->del(key);

    dumpVal.clear();

    s = client->dump(key, &dumpVal);
    EXPECT_EQ("not_found",s.code())<<"dump non existing key should not found:"<<s.code()<<endl;
    EXPECT_TRUE(dumpVal.empty())<<dumpVal<<"dump non existing key return nil:"<<endl;
    client->del(key);
}

TEST_F(RDBTest, Test_rdb_big_key_dump_restore) {
    key = "hkey";
    string restorekey = "hrestorekey";
    field = "field";
    val = "val";
    keysNum = BIG_KEY_NUM;
    keys.clear();
    kvs.clear();
    for(int n = 0;n < keysNum; n++) {
        keys.push_back(field+itoa(n));
        kvs.insert(std::make_pair(field+itoa(n), val+itoa(n)));
    }

    client->del(key);
    client->multi_hset(key, kvs);
    string dumpVal, restoreVal;
    s = client->dump(key, &dumpVal);
    cout<<dumpVal.size()<<endl;
    ASSERT_TRUE(s.ok())<<"dump big key fail!"<<s.code()<<endl;
    s = client->restore(restorekey, 0, dumpVal, "replace", &restoreVal);
    ASSERT_TRUE(s.ok())<<"restore big key fail!"<<s.code()<<endl;

    s = client->multi_hget(restorekey, keys, &getList);

    for(int n = 0;n < getList.size()/2; n++){
        if(field+itoa(n)!=getList[0+2*n]||
                val+itoa(n)!=getList[1+2*n]) {
            cout<<n<<":"<<getList[0+2*n]<<":"<<getList[1+2*n]<<endl;
            break;
        }
    }

    client->multi_del(key);
    client->multi_del(restorekey);
}

TEST_F(RDBTest, Test_rdb_big_key_mthreads_dump_restore) {
    key = "hkey";
    restorekey = "hrestorekey";
    field = "field";
    val = "val";
    keysNum = BIG_KEY_NUM;
    keys.clear();
    kvs.clear();
    for(int n = 0;n < keysNum; n++) {
        keys.push_back(field+itoa(n));
        kvs.insert(std::make_pair(field+itoa(n), val+itoa(n)));
    }

    client->del(key);
    client->multi_hset(key, kvs);
    s = client->dump(key, &dumpVal);
    cout<<dumpVal.size()<<endl;
    ASSERT_TRUE(s.ok())<<"dump big key fail!"<<s.code()<<endl;

    for(int n = 0; n < threadsNum; n++) {
        pthread_create(&bg_tid[n], NULL, &restore_thread_func, this);
        usleep(1000);
    }
        void * status;
    for(int n = 0; n < threadsNum; n++) {
        pthread_join(bg_tid[n], &status);
    }

    s = client->del(key);
    s = client->del(restorekey);
}
