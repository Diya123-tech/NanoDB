#include "include/Engine.h"
#include "include/Parser.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(x) Sleep(x)
#else
#include <unistd.h>
#define SLEEP_MS(x) usleep((x)*1000)
#endif

// ─── Timing helper ────────────────────────────────────────────────────────────
static double clockMs(){
    return (double)clock() * 1000.0 / CLOCKS_PER_SEC;
}

// ─── Process queries.txt ──────────────────────────────────────────────────────
static void processQueriesFile(NanoDBEngine& db, const char* filename){
    FILE* f=fopen(filename,"r");
    if(!f){ printf("[RUNNER] Cannot open %s\n",filename); return; }
    printf("\n========================================\n");
    printf("  Processing Workload: %s\n", filename);
    printf("========================================\n");

    char line[1024];
    int qNum=0;
    while(fgets(line, sizeof(line), f)){
        // Strip newline
        int len=(int)strlen(line);
        while(len>0&&(line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
        if(len==0||line[0]=='#') continue;

        qNum++;
        printf("\n[Q%02d] %s\n", qNum, line);

        // Enqueue into priority queue
        int pri=1;
        if(strncmp(line,"UPDATE",6)==0||strncmp(line,"INSERT",6)==0) pri=5;
        db.queryQueue.enqueue(line,pri);
    }
    fclose(f);

    printf("\n[RUNNER] Executing %d queued queries (priority order)...\n", db.queryQueue.size());
    int execNum = 0;
    while(!db.queryQueue.empty()){
        int pri;
        char* q=db.queryQueue.dequeue(&pri);
        execNum++;
        printf("\n[EXEC-%02d] (pri=%d) %s\n", execNum, pri, q);

        // ── SELECT dispatcher ──────────────────────────────────────────────
        if(strncmp(q,"SELECT",6)==0){
            const char* w=strstr(q,"WHERE");
            if(w){
                char tbl[64]="customer";
                const char* fr=strstr(q,"FROM");
                if(fr){ sscanf(fr+5,"%63s",tbl); }
                int found = db.selectWhere(tbl, w+6, false);
                printf("[RUNNER] -> %d row(s) matched.\n", found);
            }
        }

        // ── UPDATE dispatcher ──────────────────────────────────────────────
        // Syntax: UPDATE <table> SET <col>=<val> WHERE <expr>
        else if(strncmp(q,"UPDATE",6)==0){
            char tbl[64]={}, col[64]={}, valStr[128]={};
            // Parse table name
            sscanf(q+7, "%63s", tbl);
            // Find SET clause
            const char* setPtr = strstr(q,"SET");
            if(setPtr){
                sscanf(setPtr+4, "%63[^=]=%127s", col, valStr);
                // Strip trailing space from col name
                for(int i=(int)strlen(col)-1;i>=0&&col[i]==' ';i--) col[i]='\0';
            }
            const char* wPtr = strstr(q,"WHERE");
            int updated = 0;
            Table* t = db.catalog.getTable(tbl);
            if(t && setPtr && wPtr){
                char* postfix = infixToPostfix(wPtr+6, db.logFile);
                int ci = t->colIndex(col);
                for(int i=0;i<t->rows->size;i++){
                    if(evalPostfix(postfix, t->rows->data[i], t)){
                        if(ci>=0 && t->rows->data[i]->cols[ci]){
                            delete t->rows->data[i]->cols[ci];
                            // Detect int vs float vs string
                            bool isNum=true; bool hasDot=false;
                            for(int k=0;valStr[k];k++){
                                if(valStr[k]=='.') hasDot=true;
                                else if(!isdigit((unsigned char)valStr[k])&&!(k==0&&valStr[k]=='-'))
                                    isNum=false;
                            }
                            if(isNum && hasDot)
                                t->rows->data[i]->cols[ci]=new FloatValue((float)atof(valStr));
                            else if(isNum)
                                t->rows->data[i]->cols[ci]=new IntValue(atoi(valStr));
                            else
                                t->rows->data[i]->cols[ci]=new StringValue(valStr);
                            updated++;
                        }
                    }
                }
                delete[] postfix;
                fprintf(db.logFile,"[LOG] UPDATE %s SET %s=%s: %d row(s) updated.\n",
                        tbl, col, valStr, updated);
                fflush(db.logFile);
            }
            printf("[RUNNER] -> UPDATE applied to %d row(s).\n", updated);
        }

        // ── INSERT dispatcher ──────────────────────────────────────────────
        // Syntax: INSERT INTO <table> VALUES v1 v2 v3 ...
        else if(strncmp(q,"INSERT",6)==0){
            char tbl[64]={};
            const char* vPtr = strstr(q,"VALUES");
            const char* intoPtr = strstr(q,"INTO");
            if(intoPtr) sscanf(intoPtr+5,"%63s",tbl);
            Table* t = db.catalog.getTable(tbl);
            if(t && vPtr){
                // Tokenize values (space-separated)
                char valBuf[512];
                strncpy(valBuf, vPtr+7, 511); valBuf[511]='\0';
                // Split by spaces into tokens
                char* tokens[32]; int numTok=0;
                char* ptr=valBuf;
                while(*ptr && numTok<32){
                    while(*ptr==' ') ptr++;
                    if(!*ptr) break;
                    tokens[numTok++]=ptr;
                    while(*ptr && *ptr!=' ') ptr++;
                    if(*ptr){ *ptr='\0'; ptr++; }
                }
                if(numTok==t->numCols){
                    DataValue* vals[32];
                    for(int i=0;i<numTok;i++){
                        // Detect type from column definition
                        if(t->cols[i].type==TYPE_INT)
                            vals[i]=new IntValue(atoi(tokens[i]));
                        else if(t->cols[i].type==TYPE_FLOAT)
                            vals[i]=new FloatValue((float)atof(tokens[i]));
                        else
                            vals[i]=new StringValue(tokens[i]);
                    }
                    db.insertRow(tbl, vals, numTok);
                    for(int i=0;i<numTok;i++) delete vals[i];
                    printf("[RUNNER] -> INSERT into '%s' successful.\n", tbl);
                } else {
                    printf("[RUNNER] -> INSERT column count mismatch (got %d, expected %d).\n",
                           numTok, t->numCols);
                }
            }
        }

        delete[] q;
    }
    printf("\n[RUNNER] Workload complete. %d queries executed.\n", execNum);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(){
    printf("╔══════════════════════════════════════════╗\n");
    printf("║          NanoDB Engine v1.0              ║\n");
    printf("║  CS-4002 Applied Programming - MS-CS     ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    // Create data directory (works on Windows and Linux)
#ifdef _WIN32
    system("if not exist data mkdir data");
#else
    system("mkdir -p data");
#endif

    NanoDBEngine db;
    db.setupTables();

    // ── Check if saved data exists; else generate ─────────────────────────────
    {
        FILE* test=fopen("data/customer.ndb","rb");
        if(test){
            fclose(test);
            printf("[MAIN] Found existing data — loading from disk...\n");
            db.loadAll();
        } else {
            printf("[MAIN] No saved data — generating synthetic TPC-H subset (100K records)...\n");
            db.generateData(20000, 30000, 50000);
            db.persistAll();
        }
    }

    printf("\nData loaded. customer=%d rows  orders=%d rows  lineitem=%d rows\n",
           db.catalog.getTable("customer")->rows->size,
           db.catalog.getTable("orders")->rows->size,
           db.catalog.getTable("lineitem")->rows->size);

    // ══════════════════════════════════════════════════════════════════════════
    printf("\n\n");
    printf("╔══════════════════════════════════════════╗\n");
    printf("║        LIVE DEMO TEST CASES              ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    // ── TEST CASE A: Parser & Evaluator ───────────────────────────────────────
    printf("\n===== TEST CASE A: Parser & Expression Evaluator =====\n");
    {
        const char* expr="(c_acctbal > 5000 AND c_mktsegment == BUILDING) OR c_nationkey == 15";
        printf("[TEST A] Query: SELECT WHERE %s\n", expr);
        printf("[TEST A] Converting to postfix...\n");

        char* pf=infixToPostfix(expr, db.logFile);
        printf("[TEST A] Postfix: %s\n\n", pf);
        delete[] pf;

        printf("[TEST A] Evaluating against customer table...\n");
        int found=db.selectWhere("customer", expr, true);
        printf("[TEST A] Total rows matched: %d\n", found);
    }

    // ── TEST CASE B: Index vs Sequential Scan ─────────────────────────────────
    printf("\n\n===== TEST CASE B: Index Optimizer (Sequential vs AVL) =====\n");
    {
        int searchKey=10000;
        printf("[TEST B] Searching for c_custkey = %d across %d records\n",
               searchKey, db.catalog.getTable("customer")->rows->size);

        // Sequential scan
        double t0=clockMs();
        for(int rep=0;rep<1000;rep++) db.seqScan("customer",searchKey);
        double seqMs=(clockMs()-t0)/1000.0;
        printf("[TEST B] Sequential scan avg time: %.6f ms\n", seqMs);

        // AVL index scan
        double t1=clockMs();
        for(int rep=0;rep<1000;rep++) db.indexScan(searchKey);
        double idxMs=(clockMs()-t1)/1000.0;
        printf("[TEST B] AVL index scan avg time:  %.6f ms\n", idxMs);

        double speedup = seqMs > 0 ? seqMs/idxMs : 0;
        printf("[TEST B] Speedup: %.1fx  (tree height=%d)\n",
               speedup, db.custIndex.treeHeight());

        fprintf(db.logFile,"[LOG] TEST B: seq=%.6fms  avl=%.6fms  speedup=%.1fx\n",
                seqMs, idxMs, speedup);
        fflush(db.logFile);
    }

    // ── TEST CASE C: Join Optimizer ───────────────────────────────────────────
    db.joinThreeTables();

    // ── TEST CASE D: Memory Stress Test (LRU evictions) ──────────────────────
    printf("\n\n===== TEST CASE D: Memory Stress Test (LRU Buffer Pool) =====\n");
    {
        printf("[TEST D] Buffer pool capped at %d pages. Simulating page accesses...\n",
               MAX_POOL_PAGES);
        // Simulate fetching 200 distinct pages (forces evictions since pool=50)
        for(int i=0;i<200;i++){
            Page* pg=db.bufPool->fetchPage(i);
            if(pg){ pg->data[0]=(char)(i%256); db.bufPool->markDirty(i); }
        }
        int evictions=db.bufPool->getEvictions();
        printf("[TEST D] Total LRU eviction events: %d\n", evictions);
        fprintf(db.logFile,"[LOG] TEST D: %d LRU evictions during stress test.\n", evictions);
        fflush(db.logFile);
    }

    // ── TEST CASE E: Priority Queue Concurrency ───────────────────────────────
    db.priorityQueueDemo();

    // ── TEST CASE F: Deep Expression Tree ─────────────────────────────────────
    printf("\n\n===== TEST CASE F: Deep Expression Tree Edge Case =====\n");
    {
        const char* expr="( (o_totalprice * 1) > 50000 AND (o_custkey % 2 == 0) ) OR (o_orderstatus != O)";
        printf("[TEST F] Complex expression: %s\n\n", expr);
        char* pf=infixToPostfix(expr, db.logFile);
        printf("[TEST F] Postfix: %s\n\n", pf);
        delete[] pf;

        printf("[TEST F] Evaluating against orders table...\n");
        int found=db.selectWhere("orders", expr, true);
        printf("[TEST F] Total rows matched: %d\n", found);
    }

    // ── TEST CASE G: Durability & Persistence ─────────────────────────────────
    printf("\n\n===== TEST CASE G: Durability & Persistence =====\n");
    {
        Table* cust=db.catalog.getTable("customer");
        // Insert 5 new customers
        for(int i=0;i<5;i++){
            int newKey=90000+i;
            DataValue* vals[5];
            vals[0]=new IntValue(newKey);
            char nm[64]; snprintf(nm,64,"NewCust_%d",newKey);
            vals[1]=new StringValue(nm);
            vals[2]=new IntValue(i%25);
            vals[3]=new FloatValue(1234.56f);
            vals[4]=new StringValue("BUILDING");
            db.insertRow("customer", vals, 5);
            for(int j=0;j<5;j++) delete vals[j];
        }
        printf("[TEST G] Inserted 5 new customers (table now has %d rows).\n",
               cust->rows->size);

        // Save to disk
        db.persistAll();
        printf("[TEST G] Data persisted. Simulating engine restart...\n\n");

        // Simulate restart: clear and reload
        // (In a real restart the program would exit and relaunch)
        for(int i=0;i<cust->rows->size;i++) delete cust->rows->data[i];
        cust->rows->size=0;

        loadTable(cust);
        printf("[TEST G] After reload: %d rows in customer.\n", cust->rows->size);
        printf("[TEST G] Last 5 rows:\n");
        for(int i=cust->rows->size-5;i<cust->rows->size;i++)
            cust->rows->data[i]->print(cust->cols);

        fprintf(db.logFile,"[LOG] TEST G: %d rows persisted and verified after reload.\n",
                cust->rows->size);
        fflush(db.logFile);
    }

    // ── Process queries.txt workload ──────────────────────────────────────────
    processQueriesFile(db, "queries.txt");

    printf("\n\n╔══════════════════════════════════════════╗\n");
    printf("║     All Tests Complete. Log saved to:    ║\n");
    printf("║       nanodb_execution.log               ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    return 0;
}
