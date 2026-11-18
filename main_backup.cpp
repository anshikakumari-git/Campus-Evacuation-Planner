
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <algorithm>
#include <climits>
#include <sstream>
#include "httplib.h"

using namespace std;

const int N = 15;

string locations[N] = {
    "Main Gate",
    "MPH Room",
    "Ground Floor Corridor",
    "Mess",
    "Library",
    "Left Staircase Ground",
    "Right Staircase Ground",
    "Left Staircase First Floor",
    "Right Staircase First Floor",
    "First Floor Corridor",
    "Lab 1",
    "Lab 2",
    "Classrooms Left",
    "Classrooms Right",
    "Main Classroom Block"
};

vector<pair<int, int>> adj[N];

bool blocked[N][N] = {};
int congestion[N][N] = {};
int currentLocation = -1;

// Add connection
void addEdge(int u, int v, int weight) {
    adj[u].push_back({v, weight});
    adj[v].push_back({u, weight});
}

// Check connection
bool isConnected(int u, int v) {
    for (auto edge : adj[u]) {
        if (edge.first == v)
            return true;
    }
    return false;
}

// Convert string to JSON-safe text
string jsonEscape(string s) {
    string result;

    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else result += c;
    }

    return result;
}

// Get shortest route using Dijkstra
struct RouteResult {
    bool found = false;
    int cost = 0;
    vector<int> path;
};

RouteResult dijkstra(int start, int destination) {

    RouteResult result;

    vector<int> dist(N, INT_MAX);
    vector<int> parent(N, -1);

    priority_queue<
        pair<int, int>,
        vector<pair<int, int>>,
        greater<pair<int, int>>
    > pq;

    dist[start] = 0;
    pq.push({0, start});

    while (!pq.empty()) {

        int currentDistance = pq.top().first;
        int u = pq.top().second;
        pq.pop();

        if (currentDistance != dist[u])
            continue;

        for (auto edge : adj[u]) {

            int v = edge.first;
            int weight = edge.second;

            if (blocked[u][v])
                continue;

            int finalWeight = weight + congestion[u][v];

            if (dist[u] + finalWeight < dist[v]) {

                dist[v] = dist[u] + finalWeight;
                parent[v] = u;

                pq.push({dist[v], v});
            }
        }
    }

    if (dist[destination] == INT_MAX)
        return result;

    result.found = true;
    result.cost = dist[destination];

    int current = destination;

    while (current != -1) {
        result.path.push_back(current);
        current = parent[current];
    }

    reverse(result.path.begin(), result.path.end());

    return result;
}

// BFS traversal
vector<int> BFS(int start) {

    vector<int> result;
    bool visited[N] = {};

    queue<int> q;

    visited[start] = true;
    q.push(start);

    while (!q.empty()) {

        int u = q.front();
        q.pop();

        result.push_back(u);

        for (auto edge : adj[u]) {

            int v = edge.first;

            if (!blocked[u][v] && !visited[v]) {
                visited[v] = true;
                q.push(v);
            }
        }
    }

    return result;
}

// DFS helper
void DFSUtil(int u, bool visited[], vector<int>& result) {

    visited[u] = true;
    result.push_back(u);

    for (auto edge : adj[u]) {

        int v = edge.first;

        if (!blocked[u][v] && !visited[v]) {
            DFSUtil(v, visited, result);
        }
    }
}

// DFS traversal
vector<int> DFS(int start) {

    vector<int> result;
    bool visited[N] = {};

    DFSUtil(start, visited, result);

    return result;
}

// Convert traversal result to JSON
string traversalJSON(vector<int> result) {

    stringstream ss;

    ss << "{\"locations\":[";

    for (int i = 0; i < result.size(); i++) {
        if (i > 0) ss << ",";
        ss << "\"" << jsonEscape(locations[result[i]]) << "\"";
    }

    ss << "]}";

    return ss.str();
}

// Read integer parameters safely
bool getInt(const httplib::Request& req,
            const string& name, int& value) {

    if (!req.has_param(name))
        return false;

    try {
        string s = req.get_param_value(name);
        size_t pos;
        value = stoi(s, &pos);
        return pos == s.size();
    }
    catch (...) {
        return false;
    }
}

int main() {

    // Build campus graph

    addEdge(0, 2, 4);
    addEdge(2, 1, 3);
    addEdge(2, 3, 4);
    addEdge(2, 4, 6);
    addEdge(2, 5, 3);
    addEdge(2, 6, 5);

    addEdge(5, 7, 5);
    addEdge(6, 8, 5);

    addEdge(7, 9, 3);
    addEdge(8, 9, 4);
    addEdge(9, 10, 3);
    addEdge(9, 11, 4);
    addEdge(9, 12, 3);
    addEdge(9, 13, 3);
    addEdge(9, 14, 5);

    httplib::Server server;

    // Get all campus locations
    server.Get("/api/locations", [](const httplib::Request& req,
                                    httplib::Response& res) {

        stringstream ss;

        ss << "{\"locations\":[";

        for (int i = 0; i < N; i++) {
            if (i > 0) ss << ",";

            ss << "{\"id\":" << i
               << ",\"name\":\"" << jsonEscape(locations[i])
               << "\"}";
        }

        ss << "]}";

        res.set_content(ss.str(), "application/json");
    });

    // Set current location
    server.Post("/api/location", [](const httplib::Request& req,
                                    httplib::Response& res) {

        int id;

        if (!getInt(req, "id", id) || id < 0 || id >= N) {
            res.status = 400;
            res.set_content("{\"message\":\"Invalid location ID\"}",
                            "application/json");
            return;
        }

        currentLocation = id;

        res.set_content(
            "{\"message\":\"Current location updated successfully\"}",
            "application/json"
        );
    });

    // Report blocked path
    server.Post("/api/block", [](const httplib::Request& req,
                                 httplib::Response& res) {

        int u, v;

        if (!getInt(req, "u", u) || !getInt(req, "v", v) ||
            u < 0 || u >= N || v < 0 || v >= N) {

            res.status = 400;
            res.set_content("{\"message\":\"Invalid location IDs\"}",
                            "application/json");
            return;
        }

        if (!isConnected(u, v)) {
            res.status = 400;
            res.set_content("{\"message\":\"These locations are not directly connected\"}",
                            "application/json");
            return;
        }

        if (blocked[u][v]) {
            res.status = 400;
            res.set_content("{\"message\":\"Path is already blocked\"}",
                            "application/json");
            return;
        }

        blocked[u][v] = true;
        blocked[v][u] = true;

        congestion[u][v] = 0;
        congestion[v][u] = 0;

        res.set_content("{\"message\":\"Path blocked successfully\"}",
                        "application/json");
    });

    // Report congestion
    server.Post("/api/congestion", [](const httplib::Request& req,
                                      httplib::Response& res) {

        int u, v, level;

        if (!getInt(req, "u", u) || !getInt(req, "v", v) ||
            !getInt(req, "level", level) ||
            u < 0 || u >= N || v < 0 || v >= N) {

            res.status = 400;
            res.set_content("{\"message\":\"Invalid input\"}",
                            "application/json");
            return;
        }

        if (!isConnected(u, v) || blocked[u][v]) {
            res.status = 400;
            res.set_content("{\"message\":\"Invalid or blocked path\"}",
                            "application/json");
            return;
        }

        int penalty;

        if (level == 1) penalty = 2;
        else if (level == 2) penalty = 5;
        else if (level == 3) penalty = 10;
        else {
            res.status = 400;
            res.set_content("{\"message\":\"Invalid congestion level\"}",
                            "application/json");
            return;
        }

        congestion[u][v] = penalty;
        congestion[v][u] = penalty;

        res.set_content("{\"message\":\"Congestion updated successfully\"}",
                        "application/json");
    });

    // Find evacuation route
    server.Get("/api/route", [](const httplib::Request& req,
                                httplib::Response& res) {

        if (currentLocation == -1) {
            res.status = 400;
            res.set_content("{\"success\":false,\"message\":\"Please select your current location first\"}",
                            "application/json");
            return;
        }

        RouteResult result = dijkstra(currentLocation, 0);

        if (!result.found) {
            res.set_content(
                "{\"success\":false,\"message\":\"No available evacuation route\"}",
                "application/json"
            );
            return;
        }

        stringstream ss;

        ss << "{\"success\":true,\"cost\":" << result.cost
           << ",\"route\":[";

        for (int i = 0; i < result.path.size(); i++) {
            if (i > 0) ss << ",";

            ss << "\"" << jsonEscape(locations[result.path[i]]) << "\"";
        }

        ss << "]}";

        res.set_content(ss.str(), "application/json");
    });

    // BFS
    server.Get("/api/bfs", [](const httplib::Request& req,
                              httplib::Response& res) {

        if (currentLocation == -1) {
            res.status = 400;
            res.set_content("{\"message\":\"Select your current location first\"}",
                            "application/json");
            return;
        }

        res.set_content(
            traversalJSON(BFS(currentLocation)),
            "application/json"
        );
    });

    // DFS
    server.Get("/api/dfs", [](const httplib::Request& req,
                              httplib::Response& res) {

        if (currentLocation == -1) {
            res.status = 400;
            res.set_content("{\"message\":\"Select your current location first\"}",
                            "application/json");
            return;
        }

        res.set_content(
            traversalJSON(DFS(currentLocation)),
            "application/json"
        );
    });

    // Display blocked and congested paths
    server.Get("/api/conditions", [](const httplib::Request& req,
                                     httplib::Response& res) {

        stringstream ss;

        ss << "{\"blocked\":[";

        bool first = true;

        for (int i = 0; i < N; i++) {
            for (int j = i + 1; j < N; j++) {

                if (blocked[i][j]) {
                    if (!first) ss << ",";
                    first = false;

                    ss << "{\"u\":" << i << ",\"v\":" << j
                       << ",\"from\":\"" << jsonEscape(locations[i])
                       << "\",\"to\":\"" << jsonEscape(locations[j])
                       << "\"}";
                }
            }
        }

        ss << "],\"congested\":[";

        first = true;

        for (int i = 0; i < N; i++) {
            for (int j = i + 1; j < N; j++) {

                if (congestion[i][j] > 0) {
                    if (!first) ss << ",";
                    first = false;

                    ss << "{\"u\":" << i << ",\"v\":" << j
                       << ",\"from\":\"" << jsonEscape(locations[i])
                       << "\",\"to\":\"" << jsonEscape(locations[j])
                       << "\",\"penalty\":" << congestion[i][j]
                       << "}";
                }
            }
        }

        ss << "]}";

        res.set_content(ss.str(), "application/json");
    });

    // Reset all reports
    server.Post("/api/reset", [](const httplib::Request& req,
                                 httplib::Response& res) {

        for (int i = 0; i < N; i++) {
            for (int j = 0; j < N; j++) {
                blocked[i][j] = false;
                congestion[i][j] = 0;
            }
        }

        currentLocation = -1;

        res.set_content("{\"message\":\"Planner reset successfully\"}",
                        "application/json");
    });

    // Serve frontend files
    if (!server.set_mount_point("/", "./frontend")) {
        cout << "Could not find frontend folder!\n";
        return 1;
    }

    cout << "\n========================================\n";
    cout << " CAMPUS EVACUATION PLANNER\n";
    cout << "========================================\n";
    cout << "Open http://localhost:8080 in your browser\n";

    server.listen("127.0.0.1", 8080);

    return 0;
}







