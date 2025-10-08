#include "expense.h"

#include <emscripten/bind.h>
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

using namespace std;

// ---------- In-memory storage ----------
static map<string, Group> groups; // groupName -> Group
static map<string,int> groupCounters; // groupName -> next expense id int

// ---------- Helpers ----------
static string jsonEscape(const string &s) {
    string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

static vector<string> splitPipe(const string &s) {
    vector<string> parts;
    string cur;
    stringstream ss(s);
    while (getline(ss, cur, '|')) {
        if (!cur.empty()) parts.push_back(cur);
    }
    return parts;
}

static string formatAmount(double v) {
    stringstream ss;
    ss << fixed << setprecision(2) << v;
    return ss.str();
}

static bool approxEqual(double a, double b, double eps = 0.01) {
    return fabs(a - b) <= eps;
}

// ---------- Group management ----------
string createGroup(const string &groupName, const string &members_str) {
    if (groupName.empty()) return string("{\"error\":\"groupName empty\"}");
    if (groups.find(groupName) != groups.end()) {
        return string("{\"error\":\"group already exists\"}");
    }
    Group g;
    g.name = groupName;
    g.members = splitPipe(members_str);
    groups[groupName] = g;
    groupCounters[groupName] = 1;
    return string("{\"ok\":true}");
}

string listGroups() {
    stringstream ss;
    ss << "[";
    bool first = true;
    for (auto &p : groups) {
        if (!first) ss << ",";
        ss << "{\"name\":\"" << jsonEscape(p.first) << "\"}";
        first = false;
    }
    ss << "]";
    return ss.str();
}

string getGroupMembers(const string &groupName) {
    auto it = groups.find(groupName);
    if (it == groups.end()) return string("{\"error\":\"group not found\"}");
    stringstream ss;
    ss << "[";
    bool first = true;
    for (auto &m : it->second.members) {
        if (!first) ss << ",";
        ss << "\"" << jsonEscape(m) << "\"";
        first = false;
    }
    ss << "]";
    return ss.str();
}

// ---------- Expense CRUD ----------
static bool validateMembersInGroup(const Group &g, const vector<string> &members) {
    for (auto &m : members) {
        if (find(g.members.begin(), g.members.end(), m) == g.members.end()) return false;
    }
    return true;
}

string addGroupExpense(const string &groupName, const string &name, const string &category,
                       double amount, const string &payer, const string &members_str,
                       const string &shares_str, const string &date) {
    auto it = groups.find(groupName);
    if (it == groups.end()) return string("{\"error\":\"group not found\"}");
    Group &g = it->second;

    Expense e;
    e.id = to_string(groupCounters[groupName]++);
    e.name = name;
    e.category = category;
    e.amount = amount;
    e.payer = payer;
    e.date = date;
    e.members = splitPipe(members_str);

    if (e.members.empty()) return string("{\"error\":\"members empty\"}");
    if (!validateMembersInGroup(g, e.members)) return string("{\"error\":\"one or more members not in group\"}");

    // parse shares if provided
    if (!shares_str.empty()) {
        vector<string> tokens = splitPipe(shares_str);
        for (auto &t : tokens) {
            try {
                e.shares.push_back(stod(t));
            } catch(...) { return string("{\"error\":\"invalid shares format\"}"); }
        }
        if (e.shares.size() != e.members.size()) return string("{\"error\":\"shares count mismatch members count\"}");
    }

    // if no shares, equal split
    if (e.shares.empty()) {
        double equal_share = amount / (double)e.members.size();
        e.shares.assign(e.members.size(), equal_share);
    }

    // validate sum of shares ≈ amount
    double total = 0;
    for (auto v : e.shares) total += v;
    if (!approxEqual(total, amount)) {
        // return warning but still accept (frontend can show warning)
        stringstream ss;
        ss << "{\"ok\":true, \"warning\":\"total shares (" << formatAmount(total)
           << ") do not match amount (" << formatAmount(amount) << ")\", \"id\":\"" << e.id << "\"}";
        g.expenses.push_back(e);
        return ss.str();
    }

    g.expenses.push_back(e);
    stringstream ss;
    ss << "{\"ok\":true,\"id\":\"" << e.id << "\"}";
    return ss.str();
}

string findExpenseIndex(Group &g, const string &expenseId) {
    for (size_t i = 0; i < g.expenses.size(); ++i) {
        if (g.expenses[i].id == expenseId) return to_string((int)i);
    }
    return string(""); // not found
}

string editExpense(const string &groupName, const string &expenseId, const string &name,
                   const string &category, double amount, const string &payer,
                   const string &members_str, const string &shares_str, const string &date) {
    auto it = groups.find(groupName);
    if (it == groups.end()) return string("{\"error\":\"group not found\"}");
    Group &g = it->second;

    int idx = -1;
    for (size_t i = 0; i < g.expenses.size(); ++i) {
        if (g.expenses[i].id == expenseId) { idx = (int)i; break; }
    }
    if (idx == -1) return string("{\"error\":\"expense not found\"}");

    Expense &e = g.expenses[idx];
    e.name = name;
    e.category = category;
    e.amount = amount;
    e.payer = payer;
    e.date = date;
    e.members = splitPipe(members_str);

    if (e.members.empty()) return string("{\"error\":\"members empty\"}");
    if (!validateMembersInGroup(g, e.members)) return string("{\"error\":\"one or more members not in group\"}");

    e.shares.clear();
    if (!shares_str.empty()) {
        vector<string> tokens = splitPipe(shares_str);
        for (auto &t : tokens) {
            try { e.shares.push_back(stod(t)); }
            catch(...) { return string("{\"error\":\"invalid shares format\"}"); }
        }
        if (e.shares.size() != e.members.size()) return string("{\"error\":\"shares count mismatch members count\"}");
    }

    if (e.shares.empty()) {
        double equal_share = amount / (double)e.members.size();
        e.shares.assign(e.members.size(), equal_share);
    }

    double total = 0;
    for (auto v : e.shares) total += v;
    if (!approxEqual(total, amount)) {
        stringstream ss;
        ss << "{\"ok\":true, \"warning\":\"total shares (" << formatAmount(total)
           << ") do not match amount (" << formatAmount(amount) << ")\"}";
        return ss.str();
    }

    return string("{\"ok\":true}");
}

string deleteExpense(const string &groupName, const string &expenseId) {
    auto it = groups.find(groupName);
    if (it == groups.end()) return string("{\"error\":\"group not found\"}");
    Group &g = it->second;
    auto &vec = g.expenses;
    auto rem = remove_if(vec.begin(), vec.end(), [&](const Expense &e){ return e.id == expenseId; });
    if (rem == vec.end()) return string("{\"error\":\"expense not found\"}");
    vec.erase(rem, vec.end());
    return string("{\"ok\":true}");
}

// ---------- Show functions ----------
string showGroupExpenses(const string &groupName) {
    auto it = groups.find(groupName);
    if (it == groups.end()) return string("{\"error\":\"group not found\"}");
    Group &g = it->second;

    stringstream ss;
    ss << "{\"group\":\"" << jsonEscape(groupName) << "\",\"expenses\":[";
    bool first = true;
    for (auto &e : g.expenses) {
        if (!first) ss << ",";
        ss << "{";
        ss << "\"id\":\"" << jsonEscape(e.id) << "\",";
        ss << "\"name\":\"" << jsonEscape(e.name) << "\",";
        ss << "\"category\":\"" << jsonEscape(e.category) << "\",";
        ss << "\"amount\":" << formatAmount(e.amount) << ",";
        ss << "\"payer\":\"" << jsonEscape(e.payer) << "\",";
        ss << "\"members\":[";
        for (size_t i = 0; i < e.members.size(); ++i) {
            if (i) ss << ",";
            ss << "\"" << jsonEscape(e.members[i]) << "\"";
        }
        ss << "],";
        ss << "\"shares\":[";
        for (size_t i = 0; i < e.shares.size(); ++i) {
            if (i) ss << ",";
            ss << formatAmount(e.shares[i]);
        }
        ss << "],";
        ss << "\"date\":\"" << jsonEscape(e.date) << "\"";
        ss << "}";
        first = false;
    }
    ss << "]}";
    return ss.str();
}

// ---------- Settlement algorithm ----------
string calculateGroupSettlement(const string &groupName) {
    auto it = groups.find(groupName);
    if (it == groups.end()) return string("{\"error\":\"group not found\"}");
    Group &g = it->second;

    // compute balances
    map<string,double> balance;
    for (auto &m : g.members) balance[m] = 0.0;
    for (auto &e : g.expenses) {
        for (size_t i = 0; i < e.members.size(); ++i) {
            balance[e.members[i]] -= e.shares[i];
        }
        balance[e.payer] += e.amount;
    }

    // separate debtors and creditors
    vector<pair<string,double>> debtors, creditors;
    for (auto &b : balance) {
        if (b.second < -0.005) debtors.push_back({b.first, -b.second}); // owes (positive)
        else if (b.second > 0.005) creditors.push_back({b.first, b.second}); // should receive
    }

    // sort for deterministic behavior (optional)
    sort(debtors.begin(), debtors.end(), [](auto &a, auto &b){ return a.first < b.first; });
    sort(creditors.begin(), creditors.end(), [](auto &a, auto &b){ return a.first < b.first; });

    // greedy settlement
    size_t i = 0, j = 0;
    vector<tuple<string,string,double>> settlements; // debtor, creditor, amount
    while (i < debtors.size() && j < creditors.size()) {
        double pay = min(debtors[i].second, creditors[j].second);
        if (pay > 0.005) {
            settlements.emplace_back(debtors[i].first, creditors[j].first, pay);
            debtors[i].second -= pay;
            creditors[j].second -= pay;
        }
        if (debtors[i].second <= 0.005) ++i;
        if (creditors[j].second <= 0.005) ++j;
    }

    // Build JSON
    stringstream ss;
    ss << "{\"group\":\"" << jsonEscape(groupName) << "\",\"settlements\":[";
    bool first = true;
    for (auto &t : settlements) {
        if (!first) ss << ",";
        ss << "{";
        ss << "\"from\":\"" << jsonEscape(get<0>(t)) << "\",";
        ss << "\"to\":\"" << jsonEscape(get<1>(t)) << "\",";
        ss << "\"amount\":" << formatAmount(get<2>(t));
        ss << "}";
        first = false;
    }
    ss << "]}";
    return ss.str();
}

// ---------- Bindings ----------
EMSCRIPTEN_BINDINGS(expense_module) {
    emscripten::function("createGroup", &createGroup);
    emscripten::function("listGroups", &listGroups);
    emscripten::function("getGroupMembers", &getGroupMembers);

    emscripten::function("addGroupExpense", &addGroupExpense);
    emscripten::function("editExpense", &editExpense);
    emscripten::function("deleteExpense", &deleteExpense);

    emscripten::function("showGroupExpenses", &showGroupExpenses);
    emscripten::function("calculateGroupSettlement", &calculateGroupSettlement);
}