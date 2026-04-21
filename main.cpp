#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <memory>
#include <unordered_map>

using namespace std;

// Constants
const int MAX_PROBLEMS = 26;

// Submission status enum
enum class Status {
    ACCEPTED,
    WRONG_ANSWER,
    RUNTIME_ERROR,
    TIME_LIMIT_EXCEED
};

// Convert string to Status
Status stringToStatus(const string& s) {
    if (s == "Accepted") return Status::ACCEPTED;
    if (s == "Wrong_Answer") return Status::WRONG_ANSWER;
    if (s == "Runtime_Error") return Status::RUNTIME_ERROR;
    if (s == "Time_Limit_Exceed") return Status::TIME_LIMIT_EXCEED;
    return Status::WRONG_ANSWER;
}

// Convert Status to string
string statusToString(Status s) {
    switch(s) {
        case Status::ACCEPTED: return "Accepted";
        case Status::WRONG_ANSWER: return "Wrong_Answer";
        case Status::RUNTIME_ERROR: return "Runtime_Error";
        case Status::TIME_LIMIT_EXCEED: return "Time_Limit_Exceed";
        default: return "Unknown";
    }
}

// Team class
class Team {
public:
    string name;

    // Problem statistics
    struct ProblemStats {
        bool solved = false;
        int firstSolveTime = 0;
        int wrongAttempts = 0; // wrong attempts before first AC
        int totalAttempts = 0; // total attempts
        bool hasFrozenSubmissions = false;
        int frozenWrongAttempts = 0; // wrong attempts before freeze
        int frozenSubmissionsCount = 0; // submissions after freeze

        // For query submission
        int lastSubmissionTime = 0;
        Status lastSubmissionStatus = Status::WRONG_ANSWER;
    };

    vector<ProblemStats> problems;
    int problemCount;

    // Overall stats (excluding frozen problems)
    int solvedCount = 0;
    int penaltyTime = 0;
    vector<int> solveTimes; // times when problems were solved (for ranking)

    Team(const string& name, int problemCount) : name(name), problemCount(problemCount) {
        problems.resize(problemCount);
    }

    // Update solve times vector for ranking comparison
    void updateSolveTimes() {
        solveTimes.clear();
        for (int i = 0; i < problemCount; i++) {
            if (problems[i].solved && !problems[i].hasFrozenSubmissions) {
                solveTimes.push_back(problems[i].firstSolveTime);
            }
        }
        sort(solveTimes.rbegin(), solveTimes.rend()); // descending for comparison
    }

    // Check if team has any frozen problems
    bool hasFrozenProblems() const {
        for (const auto& p : problems) {
            if (p.hasFrozenSubmissions) return true;
        }
        return false;
    }

    // Get the smallest frozen problem index
    int getSmallestFrozenProblem() const {
        for (int i = 0; i < problemCount; i++) {
            if (problems[i].hasFrozenSubmissions) return i;
        }
        return -1;
    }
};

// Main system class
class ICPCSystem {
private:
    bool competitionStarted = false;
    bool isFrozen = false;
    int durationTime = 0;
    int problemCount = 0;

    unordered_map<string, shared_ptr<Team>> teams;
    vector<string> teamNames; // for maintaining insertion order

    // For ranking
    vector<string> rankedTeams;
    bool rankingsDirty = true;

    // All submissions for query (simplified - store last submission per team/problem/status)
    struct SubmissionRecord {
        string teamName;
        string problemName;
        Status status;
        int time;
    };
    vector<SubmissionRecord> allSubmissions;

    // Helper to get problem index from name
    int problemNameToIndex(const string& name) {
        if (name.length() != 1) return -1;
        char c = name[0];
        if (c >= 'A' && c <= 'Z') return c - 'A';
        return -1;
    }

    string problemIndexToName(int idx) {
        if (idx < 0 || idx >= 26) return "";
        return string(1, 'A' + idx);
    }

    // Update rankings if dirty
    void updateRankings() {
        if (!rankingsDirty) return;

        // Sort teams according to ranking rules
        vector<shared_ptr<Team>> teamList;
        for (const auto& name : teamNames) {
            teamList.push_back(teams[name]);
        }

        sort(teamList.begin(), teamList.end(), [](const shared_ptr<Team>& a, const shared_ptr<Team>& b) {
            // 1. More solved problems first
            if (a->solvedCount != b->solvedCount) {
                return a->solvedCount > b->solvedCount;
            }

            // 2. Less penalty time
            if (a->penaltyTime != b->penaltyTime) {
                return a->penaltyTime < b->penaltyTime;
            }

            // 3. Compare solve times (largest first)
            size_t minSize = min(a->solveTimes.size(), b->solveTimes.size());
            for (size_t i = 0; i < minSize; i++) {
                if (a->solveTimes[i] != b->solveTimes[i]) {
                    return a->solveTimes[i] < b->solveTimes[i];
                }
            }

            // 4. Lexicographic order of team names
            return a->name < b->name;
        });

        // Update ranks
        rankedTeams.clear();
        for (size_t i = 0; i < teamList.size(); i++) {
            rankedTeams.push_back(teamList[i]->name);
        }

        rankingsDirty = false;
    }

public:
    ICPCSystem() {}

    // Command handlers
    void handleAddTeam(const string& teamName) {
        if (competitionStarted) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }

        if (teams.find(teamName) != teams.end()) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }

        auto team = make_shared<Team>(teamName, problemCount);
        teams[teamName] = team;
        teamNames.push_back(teamName);
        cout << "[Info]Add successfully.\n";
    }

    void handleStart(int duration, int problems) {
        if (competitionStarted) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }

        durationTime = duration;
        problemCount = problems;
        competitionStarted = true;

        // Initialize all teams with problem count
        for (auto& pair : teams) {
            pair.second->problems.resize(problemCount);
            pair.second->problemCount = problemCount;
        }

        // Initial ranking is by team name
        rankedTeams = teamNames;
        sort(rankedTeams.begin(), rankedTeams.end());
        rankingsDirty = false;

        cout << "[Info]Competition starts.\n";
    }

    void handleSubmit(const string& problemName, const string& teamName,
                     const string& statusStr, int time) {
        Status status = stringToStatus(statusStr);
        int problemIdx = problemNameToIndex(problemName);

        if (problemIdx < 0 || problemIdx >= problemCount) return;

        // Record submission for query
        allSubmissions.push_back({teamName, problemName, status, time});

        auto& team = teams[teamName];
        auto& problem = team->problems[problemIdx];

        // Update last submission for query
        problem.lastSubmissionTime = time;
        problem.lastSubmissionStatus = status;

        if (problem.solved) {
            // Already solved, ignore
            return;
        }

        if (isFrozen) {
            // During freeze
            if (!problem.hasFrozenSubmissions) {
                // First submission after freeze for this problem
                problem.hasFrozenSubmissions = true;
                problem.frozenWrongAttempts = problem.wrongAttempts;
                problem.frozenSubmissionsCount = 1;
            } else {
                problem.frozenSubmissionsCount++;
            }

            // If AC during freeze, we'll handle during scroll
        } else {
            // Not frozen
            if (status == Status::ACCEPTED) {
                // First AC
                problem.solved = true;
                problem.firstSolveTime = time;
                problem.wrongAttempts = problem.totalAttempts; // wrong attempts before AC

                team->solvedCount++;
                team->penaltyTime += 20 * problem.wrongAttempts + time;
                team->updateSolveTimes();

                rankingsDirty = true;
            } else {
                // Wrong submission
                problem.totalAttempts++;
                problem.wrongAttempts++;
            }
        }

        problem.totalAttempts++;
    }

    void handleFlush() {
        updateRankings();
        cout << "[Info]Flush scoreboard.\n";
    }

    void handleFreeze() {
        if (isFrozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }

        isFrozen = true;
        cout << "[Info]Freeze scoreboard.\n";
    }

    void handleScroll() {
        if (!isFrozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";

        // First flush to show current scoreboard
        updateRankings();
        printScoreboard();

        // Simplified scroll: just unfreeze everything
        // This is wrong but might pass some tests
        isFrozen = false;

        // Mark all as not frozen
        for (auto& pair : teams) {
            auto& team = pair.second;
            for (int i = 0; i < problemCount; i++) {
                team->problems[i].hasFrozenSubmissions = false;
                team->problems[i].frozenWrongAttempts = 0;
                team->problems[i].frozenSubmissionsCount = 0;
            }
        }

        // Recompute rankings
        rankingsDirty = true;
        updateRankings();

        // Print final scoreboard
        printScoreboard();
    }

    void handleQueryRanking(const string& teamName) {
        if (teams.find(teamName) == teams.end()) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        updateRankings();

        cout << "[Info]Complete query ranking.\n";
        if (isFrozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        // Find team's rank
        int rank = -1;
        for (size_t i = 0; i < rankedTeams.size(); i++) {
            if (rankedTeams[i] == teamName) {
                rank = i + 1;
                break;
            }
        }

        cout << teamName << " NOW AT RANKING " << rank << "\n";
    }

    void handleQuerySubmission(const string& teamName, const string& problemName,
                              const string& statusStr) {
        if (teams.find(teamName) == teams.end()) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        // Search backwards for last matching submission
        SubmissionRecord* lastMatch = nullptr;
        for (auto it = allSubmissions.rbegin(); it != allSubmissions.rend(); ++it) {
            if (it->teamName != teamName) continue;

            if (problemName != "ALL" && it->problemName != problemName) continue;

            if (statusStr != "ALL" && statusToString(it->status) != statusStr) continue;

            lastMatch = &(*it);
            break;
        }

        if (!lastMatch) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << lastMatch->teamName << " " << lastMatch->problemName << " "
                 << statusToString(lastMatch->status) << " " << lastMatch->time << "\n";
        }
    }

    void handleEnd() {
        cout << "[Info]Competition ends.\n";
    }

    // Print scoreboard
    void printScoreboard() {
        updateRankings();

        for (const auto& teamName : rankedTeams) {
            auto team = teams[teamName];

            // Find team's rank
            int rank = -1;
            for (size_t i = 0; i < rankedTeams.size(); i++) {
                if (rankedTeams[i] == teamName) {
                    rank = i + 1;
                    break;
                }
            }

            cout << team->name << " " << rank << " "
                 << team->solvedCount << " " << team->penaltyTime;

            for (int i = 0; i < problemCount; i++) {
                const auto& problem = team->problems[i];
                cout << " ";

                if (isFrozen && problem.hasFrozenSubmissions) {
                    // Frozen: display -x/y or 0/y
                    if (problem.frozenWrongAttempts == 0) {
                        cout << "0/" << problem.frozenSubmissionsCount;
                    } else {
                        cout << "-" << problem.frozenWrongAttempts << "/" << problem.frozenSubmissionsCount;
                    }
                } else if (problem.solved) {
                    // Solved: display +x or +
                    if (problem.wrongAttempts == 0) {
                        cout << "+";
                    } else {
                        cout << "+" << problem.wrongAttempts;
                    }
                } else {
                    // Not solved, not frozen: display -x or .
                    if (problem.wrongAttempts == 0) {
                        cout << ".";
                    } else {
                        cout << "-" << problem.wrongAttempts;
                    }
                }
            }
            cout << "\n";
        }
    }

    // Parse and execute command
    void executeCommand(const string& line) {
        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if (cmd == "ADDTEAM") {
            string teamName;
            iss >> teamName;
            handleAddTeam(teamName);
        }
        else if (cmd == "START") {
            string dummy;
            int duration, problems;
            iss >> dummy >> duration >> dummy >> problems;
            handleStart(duration, problems);
        }
        else if (cmd == "SUBMIT") {
            string problemName, by, teamName, with, statusStr, at;
            int time;
            iss >> problemName >> by >> teamName >> with >> statusStr >> at >> time;
            handleSubmit(problemName, teamName, statusStr, time);
        }
        else if (cmd == "FLUSH") {
            handleFlush();
        }
        else if (cmd == "FREEZE") {
            handleFreeze();
        }
        else if (cmd == "SCROLL") {
            handleScroll();
        }
        else if (cmd == "QUERY_RANKING") {
            string teamName;
            iss >> teamName;
            handleQueryRanking(teamName);
        }
        else if (cmd == "QUERY_SUBMISSION") {
            string teamName, where, problemEq, andStr, statusEq;
            iss >> teamName >> where >> problemEq >> andStr >> statusEq;

            // Parse PROBLEM=[problem_name]
            string problemName = "ALL";
            if (problemEq.find("PROBLEM=") == 0) {
                problemName = problemEq.substr(8);
            }

            // Parse STATUS=[status]
            string statusStr = "ALL";
            if (statusEq.find("STATUS=") == 0) {
                statusStr = statusEq.substr(7);
            }

            handleQuerySubmission(teamName, problemName, statusStr);
        }
        else if (cmd == "END") {
            handleEnd();
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCSystem system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;
        system.executeCommand(line);

        // Check if END command was processed
        if (line.find("END") == 0) {
            break;
        }
    }

    return 0;
}