# NanoDB — Mini Database Engine
## CS-4002 Applied Programming | MS-CS Spring 2026 | FAST-NUCES Islamabad

---

## GitHub Repository
> **Link:** *(paste your GitHub repo URL here before submission)*

---

## HOW TO RUN IN DEV-C++ (Exact Steps)

### Step 1 — Extract the ZIP
Right-click the ZIP file → Extract All → you will get a folder called NanoDB.
Open that NanoDB folder. You should see main.cpp and a folder called include/ inside it.

### Step 2 — Open Dev-C++
Launch Dev-C++ from your Start menu.

### Step 3 — Open main.cpp
- Click:  File → Open  (do NOT click "New Project")
- Browse into the NanoDB folder
- Click on main.cpp
- Click Open

You will see main.cpp open in the editor. That is the only file you need to open.
All other files (inside the include/ folder) are pulled in automatically.

### Step 4 — Set Compiler Flag (do this ONE time only)
- Click:  Tools → Compiler Options
- Find the box that says "Add these commands to the compiler command line"
- Type this into that box:
    -std=c++11 -I.
- Click OK

  WHY: -std=c++11 enables C++11 features. -I. tells the compiler to look for
  header files in the current folder so it can find the include/ subfolder.
  Without -I. you will get "No such file or directory" errors on every header.

### Step 5 — Compile and Run
Press F9

A black console window will open and the program will run completely on its own.
You do not need to type anything. It runs all 7 test cases and all 50 queries automatically.

### If you see the error: "[RUNNER] Cannot open queries.txt"
This means Dev-C++ is running the .exe from the wrong folder.
Fix: make sure you opened main.cpp by going into the NanoDB folder first (Step 3).
Do not move main.cpp out of the NanoDB folder.

---

## WHAT HAPPENS WHEN YOU RUN IT

First run:
  - Generates 100,000 records (20,000 customers, 30,000 orders, 50,000 lineitems)
  - Saves them to binary files inside the data/ folder
  - Runs all 7 test cases (A through G)
  - Reads and executes all 50 queries from queries.txt
  - Saves a full log to nanodb_execution.log

Second run onwards:
  - Loads the saved data from data/ instead of regenerating (this proves Test G persistence)
  - Runs all test cases and queries again

---

## FILES IN THIS PROJECT

  main.cpp              → Open this in Dev-C++ and press F9
  queries.txt           → 50 SQL-like queries, read automatically at runtime
  nanodb_execution.log  → Generated when you run; evaluator reviews this
  data/                 → Folder where .ndb binary table files are saved
  include/
    DataValue.h         → int, float, string types with operator overloading
    Schema.h            → Row, Table, RowArray structures
    BufferPool.h        → LRU cache using Doubly Linked List + Hash Map
    Catalog.h           → System catalog using custom Hash Map
    AVLTree.h           → Self-balancing AVL Tree for indexing
    StackQueue.h        → Custom Stack, Queue, and Priority Queue (max-heap)
    Parser.h            → Shunting-Yard infix-to-postfix + expression evaluator
    JoinOptimizer.h     → Graph + Kruskal's MST for join order optimization
    FileIO.h            → Binary read/write for table persistence
    Engine.h            → Main engine: all 7 test cases + query dispatcher

---

## TEST CASES (all run automatically when you press F9)

  A — Parser:        Converts WHERE clause to Postfix, filters matching rows
  B — Index:         Times sequential scan vs AVL tree search (shows speedup)
  C — Join:          3-table join using MST to find cheapest join order
  D — Memory:        LRU buffer pool stress test, counts eviction events
  E — Priority:      Admin UPDATE jumps ahead of 15 pending user SELECTs
  F — Expression:    Deeply nested query with *, %, != operators
  G — Persistence:   Inserts 5 rows, saves to disk, reloads, verifies they exist

---

## HOW TO RUN FROM COMMAND LINE (alternative, not required for Dev-C++)

  cd NanoDB
  g++ -std=c++11 -I. -o nanodb main.cpp
  ./nanodb          (Linux or Mac)
  nanodb.exe        (Windows Command Prompt)

---

## ARCHITECTURE SUMMARY (for Viva reference)

  Component         Structure Used                  Time Complexity
  Buffer Pool       Raw fixed array (50 pages)      O(1) slot access
  LRU Eviction      Doubly Linked List + Hash Map   O(1) evict and promote
  Type System       Abstract base + virtual funcs   Polymorphism
  Catalog           Hash Map with chaining           O(1) average lookup
  Parser            Shunting-Yard + custom Stack     O(n)
  Indexing          AVL Tree (all 4 rotations)       O(log n) search/insert
  Join Optimizer    Graph edges + Kruskal's MST      O(E log E)
  Priority Queue    Max-Heap array                   O(log n) enqueue/dequeue
  File I/O          Binary fwrite / fread            Direct disk serialization

NO STL USED. Every data structure is built from scratch using raw pointers.

---

## SUBMISSION NOTES
- Replace the GitHub link at the top of this file before submitting
- Do not push .exe or data/*.ndb files to GitHub (the .gitignore blocks them)
- Make sure GitHub has multiple commits, not just one large upload
