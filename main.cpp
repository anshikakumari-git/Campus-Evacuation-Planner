
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

bool blocked[N][N] = {false};
int congestion[N][N] = {0};

int currentLocation = -1;

// Add an undirected connection
void addEdge(int u, int v, int weight) {
    adj[u].push_back({v, weight});
    adj[v].push_back({u, weight});
}

// Check direct connection
bool isConnected(int u, int v) {
    for (auto edge : adj[u]) {
        if (edge.first == v)
            return true;
    }
    return false;
}

// Convert an integer vector into JSON
string idsToJson(const vector<int>& ids) {
    stringstream ss;
    ss << "[";

    for (int i = 0; i < ids.size(); i++) {
        ss << ids[i];

        if (i != ids.size() - 1)
            ss << ",";
    }

    ss << "]";
    return ss.str();
}

// Check whether a request contains valid location IDs
bool validIds(const httplib::Request& req, int& u, int& v) {
    if (!req.has_param("u") || !req.has_param("v"))
        return false;

    try {
        u = stoi(req.get_param_value("u"));
        v = stoi(req.get_param_value("v"));
    }
    catch (...) {
        return false;
    }

    return u >= 0 && u < N && v >= 0 && v < N;
}

// Dijkstra: find shortest route to Main Gate
vector<int> dijkstra(int start, int destination, int& totalCost) {

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

    if (dist[destination] == INT_MAX) {
        totalCost = -1;
        return {};
    }

    totalCost = dist[destination];

    vector<int> path;
    int current = destination;

    while (current != -1) {
        path.push_back(current);
        current = parent[current];
    }

    reverse(path.begin(), path.end());

    return path;
}

// BFS traversal
vector<int> BFS(int start) {

    bool visited[N] = {false};
    queue<int> q;
    vector<int> result;

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

    bool visited[N] = {false};
    vector<int> result;

    DFSUtil(start, visited, result);

    return result;
}

// Reset reported conditions
void resetConditions() {

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            blocked[i][j] = false;
            congestion[i][j] = 0;
        }
    }

    currentLocation = -1;
}

int main() {

    // Ground floor
    addEdge(0, 2, 4);
    addEdge(2, 1, 3);
    addEdge(2, 3, 4);
    addEdge(2, 4, 6);
    addEdge(2, 5, 3);
    addEdge(2, 6, 5);

    // Staircases
    addEdge(5, 7, 5);
    addEdge(6, 8, 5);

    // First floor
    addEdge(7, 9, 3);
    addEdge(8, 9, 4);
    addEdge(9, 10, 3);
    addEdge(9, 11, 4);
    addEdge(9, 12, 3);
    addEdge(9, 13, 3);
    addEdge(9, 14, 5);

    httplib::Server server;

    // Send all campus locations
    server.Get("/api/locations", [](const httplib::Request& req,
                                   httplib::Response& res) {

        stringstream ss;
        ss << "[";

        for (int i = 0; i < N; i++) {
            ss << "{\"id\":" << i
               << ",\"name\":\"" << locations[i] << "\"}";

            if (i != N - 1)
                ss << ",";
        }

        ss << "]";

        res.set_content(ss.str(), "application/json");
    });

    // Set current location
    server.Post("/api/current", [](const httplib::Request& req,
                                   httplib::Response& res) {

        if (!req.has_param("id")) {
            res.status = 400;
            res.set_content("{\"message\":\"Location ID missing\"}",
                            "application/json");
            return;
        }

        int id;

        try {
            id = stoi(req.get_param_value("id"));
        }
        catch (...) {
            id = -1;
        }

        if (id < 0 || id >= N) {
            res.status = 400;
            res.set_content("{\"message\":\"Invalid location\"}",
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

        if (!validIds(req, u, v)) {
            res.status = 400;
            res.set_content("{\"message\":\"Invalid location IDs\"}",
                            "application/json");
            return;
        }

        if (!isConnected(u, v)) {
            res.set_content("{\"message\":\"These locations are not directly connected\"}",
                            "application/json");
            return;
        }

        if (blocked[u][v]) {
            res.set_content("{\"message\":\"This path is already blocked\"}",
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

        int u, v;

        if (!validIds(req, u, v) || !req.has_param("level")) {
            res.status = 400;
            res.set_content("{\"message\":\"Invalid input\"}",
                            "application/json");
            return;
        }

        if (!isConnected(u, v)) {
            res.set_content("{\"message\":\"These locations are not directly connected\"}",
                            "application/json");
            return;
        }

        if (blocked[u][v]) {
            res.set_content("{\"message\":\"This path is blocked\"}",
                            "application/json");
            return;
        }

        int level;

        try {
            level = stoi(req.get_param_value("level"));
        }
        catch (...) {
            level = 0;
        }

        int penalty;

        if (level == 1)
            penalty = 2;
        else if (level == 2)
            penalty = 5;
        else if (level == 3)
            penalty = 10;
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
            res.set_content(
                "{\"success\":false,\"message\":\"Please select your current location first\"}",
                "application/json"
            );
            return;
        }

        int cost;

        vector<int> path = dijkstra(currentLocation, 0, cost);

        if (path.empty()) {
            res.set_content(
                "{\"success\":false,\"message\":\"No available evacuation route\"}",
                "application/json"
            );
            return;
        }

        stringstream ss;

        ss << "{\"success\":true,"
           << "\"path\":" << idsToJson(path) << ","
           << "\"cost\":" << cost << "}";

        res.set_content(ss.str(), "application/json");
    });

    // BFS and DFS traversal
    server.Get("/api/traversal", [](const httplib::Request& req,
                                    httplib::Response& res) {

        if (currentLocation == -1) {
            res.set_content(
                "{\"success\":false,\"message\":\"Please select your current location first\"}",
                "application/json"
            );
            return;
        }

        if (!req.has_param("type")) {
            res.status = 400;
            res.set_content("{\"message\":\"Traversal type missing\"}",
                            "application/json");
            return;
        }

        string type = req.get_param_value("type");
        vector<int> result;

        if (type == "bfs")
            result = BFS(currentLocation);
        else if (type == "dfs")
            result = DFS(currentLocation);
        else {
            res.status = 400;
            res.set_content("{\"message\":\"Invalid traversal type\"}",
                            "application/json");
            return;
        }

        stringstream ss;

        ss << "{\"success\":true,"
           << "\"type\":\"" << type << "\","
           << "\"order\":" << idsToJson(result) << "}";

        res.set_content(ss.str(), "application/json");
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

                    if (!first)
                        ss << ",";

                    ss << "{\"u\":" << i
                       << ",\"v\":" << j << "}";

                    first = false;
                }
            }
        }

        ss << "],\"congested\":[";

        first = true;

        for (int i = 0; i < N; i++) {
            for (int j = i + 1; j < N; j++) {

                if (congestion[i][j] > 0) {

                    if (!first)
                        ss << ",";

                    ss << "{\"u\":" << i
                       << ",\"v\":" << j
                       << ",\"penalty\":" << congestion[i][j] << "}";

                    first = false;
                }
            }
        }

        ss << "]}";

        res.set_content(ss.str(), "application/json");
    });

    // Reset the application
    server.Post("/api/reset", [](const httplib::Request& req,
                                 httplib::Response& res) {

        resetConditions();

        res.set_content("{\"message\":\"Planner reset successfully\"}",
                        "application/json");
    });

    // Serve the frontend files
    if (!server.set_mount_point("/", "./frontend")) {
        cout << "Error: Frontend folder not found!\n";
        return 1;
    }

    cout << "\n========================================\n";
    cout << " CAMPUS EVACUATION PLANNER\n";
    cout << "========================================\n";
    cout << "Open your browser at:\n";
    cout << "http://localhost:8080\n";
    cout << "========================================\n";

    server.listen("127.0.0.1", 8080);

    return 0;
}








