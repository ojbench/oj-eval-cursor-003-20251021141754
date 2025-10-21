#include <bits/stdc++.h>
using namespace std;

struct ProblemState {
    // before freeze tracking
    int wrong_before = 0;           // wrong attempts before first AC or before freeze
    bool solved = false;            // solved before freeze?
    int first_ac_time = 0;          // time of first AC (if solved before freeze)

    // freeze tracking
    bool frozen = false;            // is this problem currently frozen for the team
    int submissions_after_freeze = 0; // number of submissions after freeze

    // scrolling resolution (when frozen)
    int wrong_in_freeze = 0;        // wrong submissions after freeze before first AC during freeze
    bool ac_in_freeze = false;      // did AC occur during freeze period
    int ac_time_in_freeze = 0;      // time of AC inside freeze
};

struct TeamState {
    string name;

    // live scoreboard (visible, only from non-frozen, i.e., pre-freeze events that were flushed)
    int solved_visible = 0;         // visible solved count
    long long penalty_visible = 0;  // visible total penalty
    vector<int> solve_times_visible; // multiset as vector for tie-breaker (descending comparison by max then ...)

    // per-problem state
    vector<ProblemState> problems;  // indexed 0..M-1

    // helper computed each flush: ranking index after last FLUSH
    int last_ranking = 0;
};

struct Submission {
    int team_id;
    int prob_idx; // 0..M-1
    string status; // Accepted, Wrong_Answer, Runtime_Error, Time_Limit_Exceed
    int time;
};

static inline int probIndexFromChar(char c){ return c - 'A'; }

struct RankKey {
    int solved;
    long long penalty;
    vector<int> times_desc; // sorted descending
    string name;
};

static bool rankLess(const RankKey &a, const RankKey &b){
    if (a.solved != b.solved) return a.solved > b.solved; // more solved higher
    if (a.penalty != b.penalty) return a.penalty < b.penalty; // less penalty higher
    // compare lexicographically by times in descending order: smaller max time ranks higher
    size_t n = max(a.times_desc.size(), b.times_desc.size());
    for (size_t i = 0; i < n; ++i){
        int ta = (i < a.times_desc.size() ? a.times_desc[i] : 0);
        int tb = (i < b.times_desc.size() ? b.times_desc[i] : 0);
        if (ta != tb) return ta < tb; // smaller max time ranks higher
    }
    return a.name < b.name; // name ascending
}

struct SystemState {
    bool started = false;
    bool frozen = false; // whether scoreboard is currently frozen
    int duration = 0;
    int problem_count = 0;

    vector<TeamState> teams;                // all teams
    unordered_map<string,int> name_to_id;   // map name -> team index

    vector<Submission> submissions;         // all submissions in chronological order

    // "live" scoreboard snapshot inputs come from teams[*].{solved_visible, penalty_visible, solve_times_visible}
};

static RankKey makeRankKey(const TeamState &t){
    RankKey k;
    k.solved = t.solved_visible;
    k.penalty = t.penalty_visible;
    k.times_desc = t.solve_times_visible; // already stored descending
    k.name = t.name;
    return k;
}

static void computeOrder(const SystemState &S, vector<int> &order){
    int n = (int)S.teams.size();
    order.resize(n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b){
        return rankLess(makeRankKey(S.teams[a]), makeRankKey(S.teams[b]));
    });
}

static void setLastRankingFromOrder(SystemState &S, const vector<int> &order){
    for (int i = 0; i < (int)order.size(); ++i){
        S.teams[order[i]].last_ranking = i + 1;
    }
}

static void flushScoreboard(SystemState &S){
    // just output the info line; ranking is maintained via recompute when needed
    cout << "[Info]Flush scoreboard.\n";
}

static void printScoreboard(const SystemState &S, const vector<int> &order){
    for (int idx : order){
        const TeamState &t = S.teams[idx];
        cout << t.name << ' ' << t.last_ranking << ' ' << t.solved_visible << ' ' << t.penalty_visible;
        // per problem status
        for (int p = 0; p < S.problem_count; ++p){
            const ProblemState &ps = t.problems[p];
            cout << ' ';
            if (ps.frozen){
                int x = ps.wrong_before;
                int y = ps.submissions_after_freeze;
                if (x == 0) cout << "0/" << y;
                else cout << '-' << x << '/' << y;
            } else if (ps.solved){
                int x = ps.wrong_before;
                if (x == 0) cout << '+'; else cout << '+' << x;
            } else {
                int x = ps.wrong_before;
                if (x == 0) cout << '.'; else cout << '-' << x;
            }
        }
        cout << '\n';
    }
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    SystemState S;

    string cmd;
    while (cin >> cmd){
        if (cmd == "ADDTEAM"){
            string team;
            cin >> team;
            if (S.started){
                cout << "[Error]Add failed: competition has started.\n";
                continue;
            }
            if (S.name_to_id.count(team)){
                cout << "[Error]Add failed: duplicated team name.\n";
                continue;
            }
            TeamState t;
            t.name = team;
            t.solved_visible = 0;
            t.penalty_visible = 0;
            t.solve_times_visible.clear();
            S.name_to_id[team] = (int)S.teams.size();
            S.teams.push_back(move(t));
            cout << "[Info]Add successfully.\n";
        } else if (cmd == "START"){
            string kw1, kw2; // DURATION, PROBLEM
            int duration_tmp = 0, problem_cnt_tmp = 0;
            cin >> kw1 >> duration_tmp >> kw2 >> problem_cnt_tmp;
            if (S.started){
                cout << "[Error]Start failed: competition has started.\n";
                continue;
            }
            S.started = true;
            S.duration = duration_tmp;
            S.problem_count = problem_cnt_tmp;
            // initialize problems per team
            for (auto &t : S.teams){
                t.problems.assign(S.problem_count, ProblemState());
                t.solve_times_visible.clear();
            }
            // Before first FLUSH, rankings are lexicographic by team name
            vector<int> lex_order((int)S.teams.size());
            iota(lex_order.begin(), lex_order.end(), 0);
            sort(lex_order.begin(), lex_order.end(), [&](int a, int b){ return S.teams[a].name < S.teams[b].name; });
            setLastRankingFromOrder(S, lex_order);
            cout << "[Info]Competition starts.\n";
        } else if (cmd == "SUBMIT"){
            string prob_name, by_kw, team_name, with_kw, status, at_kw; int time;
            cin >> prob_name >> by_kw >> team_name >> with_kw >> status >> at_kw >> time;
            int team_id = S.name_to_id[team_name];
            int pidx = probIndexFromChar(prob_name[0]);
            Submission sub{team_id, pidx, status, time};
            S.submissions.push_back(sub);

            TeamState &t = S.teams[team_id];
            ProblemState &ps = t.problems[pidx];

            if (!S.frozen){
                // live update to visible if not frozen state
                if (!ps.solved){
                    if (status == "Accepted"){
                        ps.solved = true;
                        ps.first_ac_time = time;
                        // penalty: 20 * wrong_before + first_ac_time
                        t.solved_visible += 1;
                        t.penalty_visible += 20LL * ps.wrong_before + ps.first_ac_time;
                        t.solve_times_visible.push_back(ps.first_ac_time);
                        sort(t.solve_times_visible.begin(), t.solve_times_visible.end(), greater<int>());
                    } else {
                        // any non-AC counts as wrong attempt
                        ps.wrong_before += 1;
                    }
                }
            } else {
                // frozen period: update per-problem frozen fields
                if (!ps.solved){
                    // if not solved before freeze, this problem becomes or stays frozen
                    if (!ps.frozen){
                        ps.frozen = true;
                    }
                    ps.submissions_after_freeze += 1;
                    if (status == "Accepted"){
                        if (!ps.ac_in_freeze){
                            ps.ac_in_freeze = true;
                            ps.ac_time_in_freeze = time;
                        }
                    } else {
                        if (!ps.ac_in_freeze){
                            ps.wrong_in_freeze += 1;
                        }
                    }
                }
            }
        } else if (cmd == "FLUSH"){
            // Recompute ranking and update last_ranking, then print info
            vector<int> order;
            computeOrder(S, order);
            setLastRankingFromOrder(S, order);
            flushScoreboard(S);
        } else if (cmd == "FREEZE"){
            if (S.frozen){
                cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            } else {
                S.frozen = true;
                cout << "[Info]Freeze scoreboard.\n";
            }
        } else if (cmd == "SCROLL"){
            if (!S.frozen){
                cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
                continue;
            }
            cout << "[Info]Scroll scoreboard.\n";
            // First, flush (update ranking) and print the scoreboard before scrolling
            vector<int> order;
            computeOrder(S, order);
            setLastRankingFromOrder(S, order);
            printScoreboard(S, order);

            // Prepare: for each team, collect list of frozen problems (by index), sorted by problem id ascending
            int n = (int)S.teams.size();
            vector<vector<int>> frozen_list(n);
            for (int i = 0; i < n; ++i){
                for (int p = 0; p < S.problem_count; ++p){
                    const auto &ps = S.teams[i].problems[p];
                    if (ps.frozen) frozen_list[i].push_back(p);
                }
            }

            auto teamHasFrozen = [&](int i){ return !frozen_list[i].empty(); };

            // Repeat until no team has frozen problems
            while (true){
                // Find lowest-ranked team with frozen problems (use current order, without touching last_ranking)
                computeOrder(S, order);
                int chosen_team = -1;
                for (int pos = (int)order.size()-1; pos >= 0; --pos){
                    int tid = order[pos];
                    if (teamHasFrozen(tid)) { chosen_team = tid; break; }
                }
                if (chosen_team == -1) break; // none left

                // Unfreeze the smallest problem id for that team
                int pidx = *min_element(frozen_list[chosen_team].begin(), frozen_list[chosen_team].end());
                // remove from list
                frozen_list[chosen_team].erase(find(frozen_list[chosen_team].begin(), frozen_list[chosen_team].end(), pidx));

                TeamState &t = S.teams[chosen_team];
                ProblemState &ps = t.problems[pidx];

                // Apply the real results to visible scoreboard
                int prev_solved = t.solved_visible;
                long long prev_penalty = t.penalty_visible;
                vector<int> prev_times = t.solve_times_visible;

                // Capture order before applying for promotion logging
                vector<int> order_before;
                computeOrder(S, order_before);
                int pos_before = -1;
                for (int i = 0; i < (int)order_before.size(); ++i){ if (order_before[i] == chosen_team) { pos_before = i; break; } }

                // resolve the frozen sequence: wrong_in_freeze then optional AC at ac_time_in_freeze
                if (!ps.solved){
                    // update wrong attempts from freeze (those before AC)
                    int wrong_add = ps.wrong_in_freeze;
                    ps.wrong_before += wrong_add;
                    if (ps.ac_in_freeze){
                        ps.solved = true;
                        ps.first_ac_time = ps.ac_time_in_freeze;
                        t.solved_visible += 1;
                        t.penalty_visible += 20LL * ps.wrong_before + ps.first_ac_time;
                        t.solve_times_visible.push_back(ps.first_ac_time);
                        sort(t.solve_times_visible.begin(), t.solve_times_visible.end(), greater<int>());
                    }
                }
                // clear frozen flags
                ps.frozen = false;
                ps.submissions_after_freeze = 0;
                ps.wrong_in_freeze = 0;
                ps.ac_in_freeze = false;
                ps.ac_time_in_freeze = 0;

                // Determine pos_after after applying
                vector<int> order_after;
                computeOrder(S, order_after);
                int pos_after = -1;
                for (int i = 0; i < (int)order_after.size(); ++i){ if (order_after[i] == chosen_team) { pos_after = i; break; } }

                // Output exactly one line per unfreeze that causes ranking change
                if (pos_before != -1 && pos_after != -1 && pos_after < pos_before){
                    int replaced_id = order_before[pos_after];
                    cout << S.teams[chosen_team].name << ' ' << S.teams[replaced_id].name << ' ' << S.teams[chosen_team].solved_visible << ' ' << S.teams[chosen_team].penalty_visible << "\n";
                }
            }

            // after scrolling all, print final scoreboard and lift frozen state
            vector<int> order_final;
            computeOrder(S, order_final);
            setLastRankingFromOrder(S, order_final);
            printScoreboard(S, order_final);
            S.frozen = false;
        } else if (cmd == "QUERY_RANKING"){
            string team; cin >> team;
            if (!S.name_to_id.count(team)){
                cout << "[Error]Query ranking failed: cannot find the team.\n";
            } else {
                cout << "[Info]Complete query ranking.\n";
                if (S.frozen) cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
                int id = S.name_to_id[team];
                cout << S.teams[id].name << " NOW AT RANKING " << S.teams[id].last_ranking << "\n";
            }
        } else if (cmd == "QUERY_SUBMISSION"){
            string team, where_kw;
            cin >> team >> where_kw;
            if (!S.name_to_id.count(team)){
                cout << "[Error]Query submission failed: cannot find the team.\n";
            } else {
                cout << "[Info]Complete query submission.\n";
                int tid = S.name_to_id[team];
                // Expect: PROBLEM=... AND STATUS=...
                string prob_token, and_kw, status_token;
                cin >> prob_token >> and_kw >> status_token;
                // Parse tokens like PROBLEM=ALL and STATUS=Accepted
                auto parse_kv = [](const string &tok, const string &prefix)->string{
                    if (tok.size() >= prefix.size()+1 && tok.rfind(prefix, 0) == 0) {
                        return tok.substr(prefix.size());
                    }
                    return string();
                };
                string prob_val = parse_kv(prob_token, "PROBLEM=");
                string status_val = parse_kv(status_token, "STATUS=");
                int target_prob = -1;
                if (!prob_val.empty() && prob_val != "ALL") target_prob = probIndexFromChar(prob_val[0]);
                string target_status = status_val.empty() ? string("ALL") : status_val;
                // Find last submission satisfying conditions (including after freeze)
                int found_idx = -1;
                for (int i = (int)S.submissions.size()-1; i >= 0; --i){
                    const auto &sub = S.submissions[i];
                    if (sub.team_id != tid) continue;
                    if (target_prob != -1 && sub.prob_idx != target_prob) continue;
                    if (target_status != "ALL" && sub.status != target_status) continue;
                    found_idx = i; break;
                }
                if (found_idx == -1){
                    cout << "Cannot find any submission.\n";
                } else {
                    const auto &sub = S.submissions[found_idx];
                    cout << S.teams[tid].name << ' ' << char('A'+sub.prob_idx) << ' ' << sub.status << ' ' << sub.time << "\n";
                }
            }
        } else if (cmd == "END"){
            cout << "[Info]Competition ends.";
            break;
        } else {
            // skip unknown (input guaranteed valid format though)
            string line; getline(cin, line);
        }
    }
    return 0;
}
