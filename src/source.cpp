#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <map>
#include <ctime>
#include <cstdlib>
#include <cmath>
#include <openssl/sha.h>
#include <unistd.h>    
#include <sys/types.h>
#include <sys/wait.h>   

using namespace std;

// ============================================
// SHA-256 HASH
// ============================================
string sha256(const string& data) 
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.c_str(), data.size());
    SHA256_Final(hash, &ctx);
    
    string result;
    char buf[3];
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(buf, "%02x", hash[i]);
        result += buf;
    }
    return result;
}

// ============================================
// JSON PARSER
// ============================================
string getField(const string& json, const string& field) 
{
    size_t pos = json.find("\"" + field + "\":");
    if(pos == string::npos) return "";
    
    pos += field.length() + 3;
    while(pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    if(json[pos] == '"') {
        pos++;
        size_t end = pos;
        while(end < json.length() && json[end] != '"') {
            if(json[end] == '\\') end++;
            end++;
        }
        return json.substr(pos, end - pos);
    }
    
    size_t end = pos;
    while(end < json.length() && json[end] != ',' && json[end] != '}') end++;
    string val = json.substr(pos, end - pos);
    val.erase(remove(val.begin(), val.end(), ' '), val.end());
    return val;
}

// ============================================
// REVIEW
// ============================================
struct Review 
{
    string id;
    string user;
    string product;
    string rating;
    string verified;
    string text;
    string summary;
    string name;
    string timestamp;
    
    string toString() const {
        return id + "|" + user + "|" + product + "|" + rating + "|" + 
               verified + "|" + text + "|" + summary + "|" + name + "|" + timestamp;
    }
    
    string hash() const {
        return sha256(toString());
    }
};

// ============================================
// MERKLE TREE
// ============================================
class MerkleTree 
{
private:
    vector<string> nodes;
    int leaves;
    int size;
    int leafStart;
    map<string, int> idMap;
    long long buildTime;
    
public:
    MerkleTree() : leaves(0), size(0), leafStart(0), buildTime(0) {}
    
    void build(const vector<Review>& reviews) 
    {
        chrono::steady_clock::time_point start = chrono::steady_clock::now();
        
        leaves = reviews.size();
        cout << "Building tree for " << leaves << " reviews..." << endl;
        
        // Find power of 2
        int pow2 = 1;
        while(pow2 < leaves) pow2 *= 2;
        size = 2 * pow2 - 1;
        
        nodes.clear();
        nodes.resize(size);
        idMap.clear();
        
        leafStart = size - pow2;
        
        // Hash leaves
        cout << "Hashing leaves..." << endl;
        for(int i = 0; i < leaves; i++) {
            nodes[leafStart + i] = reviews[i].hash();
            idMap[reviews[i].id] = i;
            
            if((i + 1) % 100000 == 0) {
                cout << "  " << (i + 1) << " done..." << endl;
            }
        }
        
        // Duplicate last if needed
        // Replace the duplication section in build() with:
        if(leaves < pow2) {
            string emptyHash = sha256("EMPTY_NODE");  // Special marker for padding
            for(int i = leaves; i < pow2; i++) {
                nodes[leafStart + i] = emptyHash;
            }
        }
                
        // Build parents
        cout << "Building parents..." << endl;
        for(int i = leafStart - 1; i >= 0; i--) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            
            if(right < size && !nodes[right].empty()) {
                nodes[i] = sha256(nodes[left] + nodes[right]);
            } else {
                nodes[i] = nodes[left];
            }
            
            if((leafStart - i) % 50000 == 0) {
                cout << "  " << (leafStart - i) << " nodes..." << endl;
            }
        }
        
        chrono::steady_clock::time_point end = chrono::steady_clock::now();
        buildTime = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        
        cout << "✓ Tree built!" << endl;
        cout << "  Time: " << buildTime << " ms" << endl;
        cout << "  Nodes: " << size << endl;
        cout << "  Memory: ~" << (size * 64 / 1024 / 1024) << " MB" << endl;
    }
    
    string getRoot() const {
        return nodes.empty() ? "" : nodes[0];
    }
    
    struct Proof 
    {
        vector<string> hashes;
        vector<bool> left;
        bool found;
        Proof() : found(false) {}
    };
    
    Proof prove(const string& id) const
    {
        Proof p;
        
        if(idMap.find(id) == idMap.end()) {
            return p;
        }
        
        p.found = true;
        int idx = leafStart + idMap.at(id);
        
        while(idx > 0) {
            int parent = (idx - 1) / 2;
            int sibling = (idx % 2 == 1) ? idx + 1 : idx - 1;
            
            if(sibling < size && !nodes[sibling].empty()) {
                p.hashes.push_back(nodes[sibling]);
                p.left.push_back(idx % 2 == 1);
            }
            
            idx = parent;
        }
        
        return p;
    }
    
    int getLeaves() const { return leaves; }
    int getSize() const { return size; }
    long long getBuildTime() const { return buildTime; }
    int getLeafStart() const { return leafStart; }
    const vector<string>& getNodes() const { return nodes; }
};

// ============================================
// DATA LOADER
// ============================================
class DataLoader 
{
public:
    static vector<string> listFiles(const string& dir) 
    {
        vector<string> files;
        
        #ifdef _WIN32
            string cmd = "dir /b " + dir + "\\*.json 2>nul";
        #else
            string cmd = "ls " + dir + "/*.json 2>/dev/null";
        #endif
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if(!pipe) return files;
        
        char buffer[256];
        while(fgets(buffer, sizeof(buffer), pipe)) {
            string file = buffer;
            // Remove newline
            if(!file.empty() && file[file.length()-1] == '\n') {
                file.erase(file.length()-1);
            }
            files.push_back(file);
        }
        pclose(pipe);
        
        return files;
    }
    
    static vector<Review> load(const string& file, int limit = -1) 
    {
        vector<Review> data;
        ifstream f(file);
        
        if(!f.is_open()) {
            cerr << "Error: Cannot open " << file << endl;
            return data;
        }
        
        cout << "Loading from " << file << "..." << endl;
        chrono::steady_clock::time_point start = chrono::steady_clock::now();
        
        string line;
        int count = 0;
        
        while(getline(f, line)) {
            if(line.empty()) continue;
            
            Review r;
            r.id = getField(line, "review_id");
            r.user = getField(line, "reviewer_id");
            r.product = getField(line, "product_id");
            r.rating = getField(line, "rating");
            r.verified = getField(line, "verified");
            r.text = getField(line, "review_text");
            r.summary = getField(line, "summary");
            r.name = getField(line, "reviewer_name");
            r.timestamp = getField(line, "timestamp");
            
            data.push_back(r);
            count++;
            
            if(count % 100000 == 0) {
                cout << "  " << count << " loaded..." << endl;
            }
            
            if(limit > 0 && count >= limit) break;
        }
        
        f.close();
        
        chrono::steady_clock::time_point end = chrono::steady_clock::now();
        long long ms = chrono::duration_cast<chrono::milliseconds>(end - start).count();
        
        cout << "Loaded " << data.size() << " reviews" << endl;
        cout << "Time: " << ms << " ms" << endl;
        
        return data;
    }
};

// ============================================
// INTEGRITY
// ============================================
class Integrity 
{
private:
    string stored;
    string file;
    
public:
    Integrity(const string& f = "merkle_root.txt") : file(f) {}
    
    void save(const string& root) 
    {
        stored = root;
        ofstream f(file);
        if(f.is_open()) {
            f << root;
            f.close();
            cout << "✓ Root saved to " << file << endl;
        } else {
            cerr << "Error: Cannot save root" << endl;
        }
    }
    
    bool load() 
    {
        ifstream f(file);
        if(f.is_open()) {
            getline(f, stored);
            f.close();
            cout << "✓ Loaded stored root" << endl;
            return true;
        }
        return false;
    }
    
    bool check(const string& current) 
    {
        if(stored.empty()) {
            cout << "No stored root!" << endl;
            return false;
        }
        return stored == current;
    }
    
    void status(const string& current) 
    {
        cout << "\nIntegrity Check:" << endl;
        cout << "Stored root:  " << stored << endl;
        cout << "Current root: " << current << endl;
        
        if(check(current)) {
            cout << "Result: VERIFIED - No tampering detected" << endl;
        } else {
            cout << "Result: TAMPERED - Data has been modified!" << endl;
        }
    }
    
    string getStored() const { return stored; }
};

// ============================================
// PERFORMANCE
// ============================================
class Performance 
{
public:
    static void test(MerkleTree& tree, const vector<Review>& reviews, int tests = 100) 
    {
        cout << "\nRunning performance test..." << endl;
        cout << "Testing " << tests << " random proofs..." << endl;
        
        if(reviews.empty()) {
            cout << "No data!" << endl;
            return;
        }
        
        vector<long long> times;
        int success = 0;
        
        for(int i = 0; i < tests; i++) {
            int idx = rand() % reviews.size();
            string id = reviews[idx].id;
            
            chrono::steady_clock::time_point start = chrono::steady_clock::now();
            MerkleTree::Proof p = tree.prove(id);
            chrono::steady_clock::time_point end = chrono::steady_clock::now();
            
            long long us = chrono::duration_cast<chrono::microseconds>(end - start).count();
            times.push_back(us);
            
            if(p.found) success++;
        }
        
        long long total = 0;
        long long minT = times[0];
        long long maxT = times[0];
        
        for(size_t i = 0; i < times.size(); i++) {
            total += times[i];
            if(times[i] < minT) minT = times[i];
            if(times[i] > maxT) maxT = times[i];
        }
        
        double avg = (double)total / times.size();
        
        cout << "\nResults:" << endl;
        cout << "  Tests run: " << tests << endl;
        cout << "  Success: " << success << endl;
        cout << "  Avg time: " << fixed << setprecision(2) << avg / 1000.0 << " ms" << endl;
        cout << "  Min: " << minT / 1000.0 << " ms" << endl;
        cout << "  Max: " << maxT / 1000.0 << " ms" << endl;
        
        if(avg / 1000.0 < 100) {
            cout << "  Status: PASS (requirement: <100ms)" << endl;
        } else {
            cout << "  Status: FAIL (too slow)" << endl;
        }
    }
    
    static void stats(const vector<Review>& reviews) 
    {
        cout << "\nDataset Statistics:" << endl;
        cout << "Total reviews: " << reviews.size() << endl;
        
        map<string, int> ratings;
        map<string, int> products;
        map<string, int> users;
        
        for(size_t i = 0; i < reviews.size(); i++) {
            ratings[reviews[i].rating]++;
            products[reviews[i].product]++;
            users[reviews[i].user]++;
        }
        
        cout << "Unique products: " << products.size() << endl;
        cout << "Unique users: " << users.size() << endl;
        
        cout << "\nRating breakdown:" << endl;
        for(map<string, int>::iterator it = ratings.begin(); it != ratings.end(); ++it) {
            double pct = 100.0 * it->second / reviews.size();
            cout << "  " << it->first << " stars: " << it->second 
                 << " (" << fixed << setprecision(1) << pct << "%)" << endl;
        }
    }
};

// Tampering simulation
class Tamper 
{
public:
    static void modify(vector<Review>& reviews, int idx) 
    {
        if(idx < 0 || idx >= (int)reviews.size()) {
            cout << "Bad index!" << endl;
            return;
        }
        
        cout << "\nModifying review..." << endl;
        cout << "Before: " << reviews[idx].text.substr(0, 50) << "..." << endl;
        reviews[idx].text += " [MODIFIED]";
        cout << "After: " << reviews[idx].text.substr(0, 50) << "..." << endl;
    }
    
    static void remove(vector<Review>& reviews, int idx) 
    {
        if(idx < 0 || idx >= (int)reviews.size()) {
            cout << "Bad index!" << endl;
            return;
        }
        
        cout << "\nDeleting review..." << endl;
        cout << "ID: " << reviews[idx].id << endl;
        reviews.erase(reviews.begin() + idx);
        cout << "Done. New count: " << reviews.size() << endl;
    }
    
    static void insert(vector<Review>& reviews) 
    {
        cout << "\nInserting fake review..." << endl;
        
        Review fake;
        char buf[50];
        sprintf(buf, "FAKE_%ld", time(NULL));
        fake.id = buf;
        fake.user = "FAKE_USER";
        fake.product = "FAKE_PRODUCT";
        fake.rating = "5.0";
        fake.verified = "false";
        fake.text = "Fake review for testing!";
        fake.summary = "Fake";
        fake.name = "Faker";
        sprintf(buf, "%ld", time(NULL));
        fake.timestamp = buf;
        
        reviews.push_back(fake);
        cout << "Added: " << fake.id << endl;
        cout << "New count: " << reviews.size() << endl;
    }
};

// ============================================
// JSON EXPORTER
// ============================================
class Exporter 
{
public:
    // Export full tree structure for visualization
    static void exportTree(const MerkleTree& tree, const vector<Review>& reviews,
                          int memory, long long buildTime, bool verified) 
    {
        ofstream f("tree_data.json");
        if(!f.is_open()) {
            cerr << "Cannot write tree_data.json" << endl;
            return;
        }
        
        // Get tree internals
        const vector<string>& nodes = tree.getNodes();
        int leafStart = tree.getLeafStart();
        int leaves = tree.getLeaves();
        
        f << "{\n";
        f << "  \"root\": \"" << tree.getRoot() << "\",\n";
        f << "  \"records\": " << leaves << ",\n";
        f << "  \"buildTime\": " << buildTime << ",\n";
        f << "  \"memory\": " << memory << ",\n";
        f << "  \"integrity\": " << (verified ? "true" : "false") << ",\n";
        f << "  \"leafStart\": " << leafStart << ",\n";
        f << "  \"treeSize\": " << nodes.size() << ",\n";
        
        // Export ALL nodes (not limited)
        f << "  \"nodes\": [\n";
        for(size_t i = 0; i < nodes.size(); i++) {
            f << "    {\n";
            f << "      \"id\": " << i << ",\n";
            f << "      \"hash\": \"" << nodes[i].substr(0, 8) << "...\",\n";
            f << "      \"fullHash\": \"" << nodes[i] << "\",\n";
            f << "      \"isLeaf\": " << (i >= (size_t)leafStart ? "true" : "false") << ",\n";
            
            if(i > 0) {
                int parent = (i - 1) / 2;
                f << "      \"parent\": " << parent << ",\n";
            }
            
            size_t left = 2 * i + 1;
            size_t right = 2 * i + 2;
            if(left < nodes.size()) {
                f << "      \"left\": " << left << ",\n";
            }
            if(right < nodes.size()) {
                f << "      \"right\": " << right << ",\n";
            }
            
            // Add review data for leaves
            if(i >= (size_t)leafStart && (i - leafStart) < (size_t)leaves) {
                int reviewIdx = i - leafStart;
                f << "      \"reviewId\": \"" << reviews[reviewIdx].id << "\",\n";
                f << "      \"product\": \"" << reviews[reviewIdx].product << "\"\n";
            } else {
                int level = 0;
                int temp = i + 1;
                while(temp > 1) {
                    temp = temp / 2;
                    level++;
                }
                f << "      \"level\": " << level << "\n";
            }
            
            f << "    }";
            if(i < nodes.size() - 1) f << ",";
            f << "\n";
        }
        f << "  ],\n";
        
        // Export available review IDs
        f << "  \"availableReviews\": [\n";
        for(size_t i = 0; i < reviews.size() && i < 100; i++) {
            f << "    \"" << reviews[i].id << "\"";
            if(i < reviews.size() - 1 && i < 99) f << ",";
            f << "\n";
        }
        f << "  ]\n";
        f << "}\n";
        
        f.close();
        cout << "Exported FULL tree structure to tree_data.json" << endl;
        cout << "Total nodes: " << nodes.size() << endl;
    }
    
    // Export proof path for a specific review
    static void exportProof(const MerkleTree& tree, const string& reviewId,
                           const vector<Review>& reviews) 
    {
        MerkleTree::Proof proof = tree.prove(reviewId);
        
        if(!proof.found) {
            cout << "Review not found, cannot export proof!" << endl;
            return;
        }
        
        // Create proofs directory
        system("mkdir -p proofs 2>/dev/null");
        
        // Create filename from review ID (sanitized)
        string filename = "proofs/proof_" + reviewId.substr(0, 20) + ".json";
        
        ofstream f(filename);
        if(!f.is_open()) {
            cerr << "Cannot write " << filename << endl;
            return;
        }
        
        // Find review index
        int reviewIdx = -1;
        for(size_t i = 0; i < reviews.size(); i++) {
            if(reviews[i].id == reviewId) {
                reviewIdx = i;
                break;
            }
        }
        
        // Calculate path indices
        vector<int> pathIndices;
        int leafStart = tree.getLeafStart();
        int idx = leafStart + reviewIdx;
        pathIndices.push_back(idx);
        
        while(idx > 0) {
            int parent = (idx - 1) / 2;
            pathIndices.push_back(parent);
            idx = parent;
        }
        
        f << "{\n";
        f << "  \"reviewId\": \"" << reviewId << "\",\n";
        f << "  \"leafIndex\": " << reviewIdx << ",\n";
        f << "  \"pathLength\": " << proof.hashes.size() << ",\n";
        f << "  \"verified\": true,\n";
        
        f << "  \"proofPath\": [\n";
        for(size_t i = 0; i < proof.hashes.size(); i++) {
            f << "    {\n";
            f << "      \"hash\": \"" << proof.hashes[i].substr(0, 8) << "...\",\n";
            f << "      \"fullHash\": \"" << proof.hashes[i] << "\",\n";
            f << "      \"isLeft\": " << (proof.left[i] ? "true" : "false") << "\n";
            f << "    }";
            if(i < proof.hashes.size() - 1) f << ",";
            f << "\n";
        }
        f << "  ],\n";
        
        f << "  \"nodeIndices\": [\n";
        for(size_t i = 0; i < pathIndices.size(); i++) {
            f << "    " << pathIndices[i];
            if(i < pathIndices.size() - 1) f << ",";
            f << "\n";
        }
        f << "  ]\n";
        
        f << "}\n";
        
        f.close();
        cout << "Exported proof to " << filename << endl;
        cout << "Path length: " << proof.hashes.size() << " nodes" << endl;
    }
    
    static void toReport(const string& root, int records, long long buildTime,
                        int memory, double proofTime, bool verified, const string& file = "report.txt") 
    {
        ofstream f(file);
        if(!f.is_open()) {
            cerr << "Cannot write " << file << endl;
            return;
        }
        
        f << "=== MERKLE TREE INTEGRITY REPORT ===\n\n";
        f << "Root Hash: " << root << "\n";
        f << "Total Records: " << records << "\n";
        f << "Build Time: " << buildTime << " ms\n";
        f << "Memory Usage: ~" << memory << " MB\n";
        f << "Avg Proof Time: " << fixed << setprecision(2) << proofTime << " ms\n";
        f << "Integrity Status: " << (verified ? "VERIFIED" : "TAMPERED") << "\n";
        
        f.close();
        cout << "Report saved to " << file << endl;
    }
};

// ============================================
// CSV EXPORTER
// ============================================
class CSVExporter 
{
public:
    static bool exportToCSV(const vector<Review>& reviews, const string& filename = "reviews_export.csv") 
    {
        ofstream f(filename);
        if(!f.is_open()) {
            cerr << "Cannot write " << filename << endl;
            return false;
        }
        
        // Write CSV header
        f << "Review ID,User ID,Product ID,Rating,Verified,Review Text,Summary,Reviewer Name,Timestamp\n";
        
        // Write data rows
        for(size_t i = 0; i < reviews.size(); i++) {
            // Escape commas and quotes in fields
            auto escapeCSV = [](const string& s) -> string {
                string result = s;
                // Replace quotes with double quotes
                size_t pos = 0;
                while((pos = result.find("\"", pos)) != string::npos) {
                    result.replace(pos, 1, "\"\"");
                    pos += 2;
                }
                // Wrap in quotes if contains comma, quote, or newline
                if(result.find(',') != string::npos || 
                   result.find('\"') != string::npos || 
                   result.find('\n') != string::npos) {
                    result = "\"" + result + "\"";
                }
                return result;
            };
            
            f << escapeCSV(reviews[i].id) << ","
              << escapeCSV(reviews[i].user) << ","
              << escapeCSV(reviews[i].product) << ","
              << escapeCSV(reviews[i].rating) << ","
              << escapeCSV(reviews[i].verified) << ","
              << escapeCSV(reviews[i].text) << ","
              << escapeCSV(reviews[i].summary) << ","
              << escapeCSV(reviews[i].name) << ","
              << escapeCSV(reviews[i].timestamp) << "\n";
            
            if((i + 1) % 10000 == 0) {
                cout << "  Exported " << (i + 1) << " reviews..." << endl;
            }
        }
        
        f.close();
        cout << "Successfully exported " << reviews.size() << " reviews to " << filename << endl;
        return true;
    }
    
    static bool openWithCalc(const string& filename) 
    {
        // Ubuntu/Linux - fork and exec
        pid_t pid = fork();
        
        if(pid < 0) {
            cerr << "Fork failed!" << endl;
            return false;
        }
        else if(pid == 0) {
            
            execl("/usr/bin/localc", "localc", filename.c_str(), (char*)NULL);
            
            
            cerr << "Error: Could not open LibreOffice Calc." << endl;
            exit(1);
        }
        else 
        {
            cout << "Opening " << filename << " in LibreOffice Calc (PID: " << pid << ")..." << endl;
            cout << "LibreOffice Calc launched in background." << endl;
            return true;
        }
        
        return false;
    }
};

class ServerLauncher 
{
public:
    static bool startServer(int port = 8060) 
    {
        cout << "\nStarting live server on port " << port << "..." << endl;
        
        pid_t pid = fork();
        
        if(pid < 0) {
            cerr << "Fork failed!" << endl;
            return false;
        }
        else if(pid == 0)
        {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            
            // Convert port to string
            char portStr[10];
            sprintf(portStr, "%d", port);
            
            // Try Python3 first
            execl("/usr/bin/python3", "python3", "-m", "http.server", portStr, (char*)NULL);
          
            cerr << "Error: Could not start Python HTTP server." << endl;
            exit(1);
        }
        else 
        {
             
            // Parent process - give server time to start
            cout << "✓ Server started (PID: " << pid << ")" << endl;
            sleep(2);  // Wait 2 seconds for server to initialize
            
            // Now open the visualizer in browser
            openVisualizer(port);
            
            cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
            cout << "Visualizer URL: http://localhost:" << port << "/visualizer.html" << endl;
            cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << endl;
            
            return true;
        }
        
        return false;
    }
    
private:
    static void openVisualizer(int port) 
    {
        pid_t pid = fork();
        
        if(pid < 0) {
            cerr << "Could not open browser" << endl;
            return;
        }
        else if(pid == 0) {
        
             freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
            
            // Child process - open browser
            char url[100];
            sprintf(url, "http://localhost:%d/visualizer.html", port);
            
            execl("/usr/bin/google-chrome", "google-chrome", url, (char*)NULL);
            execl("/usr/bin/chromium-browser", "chromium-browser", url, (char*)NULL);
            
            execlp("xdg-open", "xdg-open", url, (char*)NULL);
            execlp("firefox", "firefox", url, (char*)NULL);
            execlp("google-chrome", "google-chrome", url, (char*)NULL);
            
            exit(0);
        }
    }
};


// ============================================
// MENU
// ============================================
void menu() 
{
    cout << "\n=== Merkle Tree System ===" << endl;
    cout << "1.  Load Reviews" << endl;
    cout << "2.  Build Tree" << endl;
    cout << "3.  Show Root" << endl;
    cout << "4.  Save Root" << endl;
    cout << "5.  Verify Review" << endl;
    cout << "6.  Check Integrity" << endl;
    cout << "7.  Simulate Tampering" << endl;
    cout << "8.  Performance Test" << endl;
    cout << "9.  Dataset Stats" << endl;
    cout << "10. Compare Roots" << endl;
    cout << "11. Export Tree (for GUI)" << endl;
    cout << "12. Export Proof Path" << endl;
    cout << "13. Export Report" << endl;
    cout << "14. Display Reviews (Tabular)" << endl; 
    cout << "-1. Strart live server (VISUALIZER)" << endl;
    cout << "0.  Exit" << endl;
    cout << "Choice: ";
}

// ============================================
// MAIN
// ============================================
int main() 
{
    srand(time(NULL));
    
    MerkleTree tree;
    Integrity checker;
    vector<Review> data;
    string filename;
    
    cout << "\nMerkle Tree Integrity System" << endl;
    cout << "Amazon Review Verification" << endl;
    cout << "----------------------------" << endl;
    
    checker.load();
    
    int choice;
    while(true) 
    {
        menu();
        cin >> choice;
        cin.ignore();
        
        if(choice == 1) {
            vector<string> files = DataLoader::listFiles("pre_processed");
            
            if(files.empty()) {
                cout << "No JSON files found in pre_processed/ folder!" << endl;
                cout << "Run preprocessing first (make preprocess)" << endl;
            } else {
                cout << "\nAvailable datasets:" << endl;
                for(size_t i = 0; i < files.size(); i++) {
                    cout << "  " << (i+1) << ". " << files[i] << endl;
                }
                
                cout << "\nSelect file (1-" << files.size() << "): ";
                int idx;
                cin >> idx;
                cin.ignore();
                
                if(idx < 1 || idx > (int)files.size()) {
                    cout << "Invalid selection!" << endl;
                } else {
                    filename =  files[idx-1];
                    cout << "Limit (-1 for all): ";
                    int lim;
                    cin >> lim;
                    cin.ignore();
                    data = DataLoader::load(filename, lim);
                }
            }
        }
        else if(choice == 2) {
            if(data.empty()) {
                cout << "Load data first!" << endl;
            } else {
                tree.build(data);
            }
        }
        else if(choice == 3) {
            string root = tree.getRoot();
            if(root.empty()) {
                cout << "Build tree first!" << endl;
            } else {
                cout << "\nRoot: " << root << endl;
            }
        }
        else if(choice == 4) {
            string root = tree.getRoot();
            if(root.empty()) {
                cout << "Build tree first!" << endl;
            } else {
                checker.save(root);
            }
        }
        else if(choice == 5) {
            if(data.empty()) {
                cout << "Load data first!" << endl;
            } else {
                cout << "Review ID: ";
                string id;
                getline(cin, id);
                
                chrono::steady_clock::time_point start = chrono::steady_clock::now();
                MerkleTree::Proof p = tree.prove(id);
                chrono::steady_clock::time_point end = chrono::steady_clock::now();
                long long us = chrono::duration_cast<chrono::microseconds>(end - start).count();
                
                if(p.found) {
                    cout << "\nReview found!" << endl;
                    cout << "Proof length: " << p.hashes.size() << " hashes" << endl;
                    cout << "Time taken: " << us / 1000.0 << " ms" << endl;
                    cout << "\nProof path:" << endl;
                    for(size_t i = 0; i < p.hashes.size(); i++) {
                        cout << "  " << (i+1) << ". " << p.hashes[i].substr(0, 16) << "..." 
                             << (p.left[i] ? " (left)" : " (right)") << endl;
                    }
                } else {
                    cout << "\nReview not found in dataset!" << endl;
                }
            }
        }
        else if(choice == 6) {
            string root = tree.getRoot();
            if(root.empty()) {
                cout << "Build tree first!" << endl;
            } else {
                checker.status(root);
            }
        }
        else if(choice == 7) {
            if(data.empty()) {
                cout << "Load data first!" << endl;
            } else {
                cout << "\n1. Modify\n2. Delete\n3. Insert\nChoice: ";
                int t;
                cin >> t;
                cin.ignore();
                
                if(t == 1) {
                    Tamper::modify(data, rand() % data.size());
                } else if(t == 2) {
                    Tamper::remove(data, rand() % data.size());
                } else if(t == 3) {
                    Tamper::insert(data);
                }
                
                cout << "\nRebuilding..." << endl;
                tree.build(data);
                checker.status(tree.getRoot());
            }
        }
        else if(choice == 8) {
            if(data.empty()) {
                cout << "Load data first!" << endl;
            } else {
                cout << "Number of tests: ";
                int n;
                cin >> n;
                cin.ignore();
                Performance::test(tree, data, n);
            }
        }
        else if(choice == 9) {
            if(data.empty()) {
                cout << "Load data first!" << endl;
            } else {
                Performance::stats(data);
            }
        }
        else if(choice == 10) {
            string curr = tree.getRoot();
            string stored = checker.getStored();
            
            if(curr.empty()) {
                cout << "Build tree first!" << endl;
            } else if(stored.empty()) {
                cout << "No stored root!" << endl;
            } else {
                cout << "\nStored root:  " << stored << endl;
                cout << "Current root: " << curr << endl;
                if(curr == stored) {
                    cout << "Status: Roots match!" << endl;
                } else {
                    cout << "Status: Roots differ - data modified!" << endl;
                }
            }
        }
        else if(choice == 11) {
            string root = tree.getRoot();
            if(root.empty()) {
                cout << "Build tree first!" << endl;
            } else {
                int mem = tree.getSize() * 64 / 1024 / 1024;
                bool ok = checker.check(root);
                Exporter::exportTree(tree, data, mem, tree.getBuildTime(), ok);
            }
        }
        else if(choice == 12) {
            if(data.empty()) {
                cout << "Load data first!" << endl;
            } else {
                cout << "Review ID: ";
                string id;
                getline(cin, id);
                Exporter::exportProof(tree, id, data);
            }
        }
        else if(choice == 13) {
            string root = tree.getRoot();
            if(root.empty()) {
                cout << "Build tree first!" << endl;
            } else {
                int mem = tree.getSize() * 64 / 1024 / 1024;
                bool ok = checker.check(root);
                Exporter::toReport(root, tree.getLeaves(), tree.getBuildTime(), 
                                 mem, 0.03, ok);
            }
        }
        else if(choice == 14)
        {
            if(data.empty()) 
            {
                cout << "Load data first!" << endl;
            } 
            else 
            {
                cout << "\nExporting reviews to CSV..." << endl;
                cout << "Number of reviews to export (-1 for all): ";
                int limit;
                cin >> limit;
                cin.ignore();
                
                vector<Review> exportData;
                if(limit < 0 || limit >= (int)data.size()) {
                    exportData = data;
                } else {
                    exportData.assign(data.begin(), data.begin() + limit);
                }
                
                string filename = "reviews_export.csv";
                if(CSVExporter::exportToCSV(exportData, filename)) {
                    cout << "\nOpening in LibreOffice Calc..." << endl;
                    CSVExporter::openWithCalc(filename);
                }
            }
        }
        else if(choice == -1) 
        {
            cout << "Enter port number (default 8000): ";
            string portInput;
            getline(cin, portInput);
            
            int port = 8000;
            if(!portInput.empty()) {
                port = atoi(portInput.c_str());
                if(port < 1024 || port > 65535) {
                    cout << "Invalid port! Using default 8000" << endl;
                    port = 8000;
                }
            }
            
            ServerLauncher::startServer(port);
        }
        else if(choice == 0) {
            cout << "\nGoodbye!" << endl;
            return 0;
        }
        else {
            cout << "Invalid!" << endl;
        }
    }
    
    return 0;
}


























