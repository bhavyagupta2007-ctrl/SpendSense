#include "expense.h"
#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

using namespace std;

static map<string, Group> groups;
static map<string, int> groupCounters;
static string jsonBuffer; // static buffer for returning strings safely

static string jsonEscape(const string &s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
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
    stringstream ss(s);
    string token;
    while (getline(ss, token, '|')) {
        if (!token.empty()) parts.push_back(token);
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

static bool validateMembersInGroup(const Group &g, const vector<string> &members) {
    for (auto &m : members)
        if (find(g.members.begin(), g.members.end(), m) == g.members.end())
            return false;
    return true;
}

static const char* makeJson(const string &msg) {
    jsonBuffer = msg;
    return jsonBuffer.c_str();
}

// -------------- Group Management ----------------

extern "C" const char* createGroup(const char* groupName, const char* members_str) {
    string name(groupName);
    string members(members_str);

    if (name.empty()) return makeJson("{\"error\":\"Group name empty\"}");
    if (groups.find(name) != groups.end())
        return makeJson("{\"error\":\"Group already exists\"}");

    Group g;
    g.name = name;
    g.members = splitPipe(members);
    groups[name] = g;
    groupCounters[name] = 1;

    return makeJson("{\"ok\":true}");
}

extern "C" const char* listGroups() {
    stringstream ss;
    ss << "[";
    bool first = true;
    for (auto &p : groups) {
        if (!first) ss << ",";
        ss << "{\"name\":\"" << jsonEscape(p.first) << "\"}";
        first = false;
    }
    ss << "]";
    return makeJson(ss.str());
}

extern "C" const char* getGroupMembers(const char* groupName) {
    string name(groupName);
    auto it = groups.find(name);
    if (it == groups.end()) return makeJson("{\"error\":\"Group not found\"}");

    stringstream ss;
    ss << "[";
    bool first = true;
    for (auto &m : it->second.members) {
        if (!first) ss << ",";
        ss << "\"" << jsonEscape(m) << "\"";
        first = false;
    }
    ss << "]";
    return makeJson(ss.str());
}

// -------------- Expense Management ----------------

extern "C" const char* addGroupExpense(const char* groupName, const char* name, const char* category,
                                       double amount, const char* payer, const char* members_str,
                                       const char* shares_str, const char* date) {
    string gname(groupName);
    auto it = groups.find(gname);
    if (it == groups.end()) return makeJson("{\"error\":\"Group not found\"}");
    Group &g = it->second;

    Expense e;
    e.id = to_string(groupCounters[gname]++);
    e.name = name;
    e.category = category;
    e.amount = amount;
    e.payer = payer;
    e.date = date;
    e.members = splitPipe(members_str);

    if (e.members.empty()) return makeJson("{\"error\":\"Members empty\"}");
    if (!validateMembersInGroup(g, e.members))
        return makeJson("{\"error\":\"Invalid member(s)\"}");

    vector<string> shareTokens = splitPipe(shares_str);
    if (!shareTokens.empty()) {
        for (auto &t : shareTokens) e.shares.push_back(stod(t));
        if (e.shares.size() != e.members.size())
            return makeJson("{\"error\":\"Share count mismatch\"}");
    } else {
        double equal = amount / (double)e.members.size();
        e.shares.assign(e.members.size(), equal);
    }

    double total = 0; for (auto s : e.shares) total += s;
    if (!approxEqual(total, amount)) {
        stringstream ss;
        ss << "{\"ok\":true,\"warning\":\"Shares (" << formatAmount(total)
           << ") != Amount (" << formatAmount(amount) << ")\"}";
        g.expenses.push_back(e);
        return makeJson(ss.str());
    }

    g.expenses.push_back(e);
    return makeJson("{\"ok\":true}");
}

extern "C" const char* editExpense(const char* groupName, const char* expenseId,
                                   const char* name, const char* category, double amount,
                                   const char* payer, const char* members_str,
                                   const char* shares_str, const char* date) {
    string gname(groupName);
    auto it = groups.find(gname);
    if (it == groups.end()) return makeJson("{\"error\":\"Group not found\"}");
    Group &g = it->second;

    for (auto &e : g.expenses) {
        if (e.id == expenseId) {
            e.name = name;
            e.category = category;
            e.amount = amount;
            e.payer = payer;
            e.date = date;
            e.members = splitPipe(members_str);
            e.shares.clear();

            vector<string> shareTokens = splitPipe(shares_str);
            if (!shareTokens.empty()) {
                for (auto &t : shareTokens) e.shares.push_back(stod(t));
                if (e.shares.size() != e.members.size())
                    return makeJson("{\"error\":\"Share count mismatch\"}");
            } else {
                double equal = amount / (double)e.members.size();
                e.shares.assign(e.members.size(), equal);
            }
            return makeJson("{\"ok\":true}");
        }
    }
    return makeJson("{\"error\":\"Expense not found\"}");
}

extern "C" const char* deleteExpense(const char* groupName, const char* expenseId) {
    string gname(groupName);
    auto it = groups.find(gname);
    if (it == groups.end()) return makeJson("{\"error\":\"Group not found\"}");
    Group &g = it->second;

    auto &vec = g.expenses;
    auto rem = remove_if(vec.begin(), vec.end(), [&](const Expense &e){ return e.id == expenseId; });
    if (rem == vec.end()) return makeJson("{\"error\":\"Expense not found\"}");

    vec.erase(rem, vec.end());
    return makeJson("{\"ok\":true}");
}

extern "C" const char* showGroupExpenses(const char* groupName) {
    string gname(groupName);
    auto it = groups.find(gname);
    if (it == groups.end()) return makeJson("{\"error\":\"Group not found\"}");
    Group &g = it->second;

    stringstream ss;
    ss << "{\"group\":\"" << jsonEscape(gname) << "\",\"expenses\":[";
    bool first = true;
    for (auto &e : g.expenses) {
        if (!first) ss << ",";
        ss << "{";
        ss << "\"id\":\"" << e.id << "\",";
        ss << "\"name\":\"" << jsonEscape(e.name) << "\",";
        ss << "\"category\":\"" << jsonEscape(e.category) << "\",";
        ss << "\"amount\":" << formatAmount(e.amount) << ",";
        ss << "\"payer\":\"" << jsonEscape(e.payer) << "\",";
        ss << "\"members\":[";
        for (size_t i = 0; i < e.members.size(); ++i) {
            if (i) ss << ",";
            ss << "\"" << jsonEscape(e.members[i]) << "\"";
        }
        ss << "],\"shares\":[";
        for (size_t i = 0; i < e.shares.size(); ++i) {
            if (i) ss << ",";
            ss << formatAmount(e.shares[i]);
        }
        ss << "],\"date\":\"" << jsonEscape(e.date) << "\"}";
        first = false;
    }
    ss << "]}";
    return makeJson(ss.str());
}

extern "C" const char* calculateGroupSettlement(const char* groupName) {
    string gname(groupName);
    auto it = groups.find(gname);
    if (it == groups.end()) return makeJson("{\"error\":\"Group not found\"}");
    Group &g = it->second;

    map<string, double> balance;
    for (auto &m : g.members) balance[m] = 0.0;

    for (auto &e : g.expenses) {
        for (size_t i = 0; i < e.members.size(); ++i)
            balance[e.members[i]] -= e.shares[i];
        balance[e.payer] += e.amount;
    }

    vector<pair<string, double>> debtors, creditors;
    for (auto &b : balance) {
        if (b.second < -0.005) debtors.push_back({b.first, -b.second});
        else if (b.second > 0.005) creditors.push_back({b.first, b.second});
    }

    size_t i = 0, j = 0;
    vector<tuple<string, string, double>> settlements;
    while (i < debtors.size() && j < creditors.size()) {
        double pay = min(debtors[i].second, creditors[j].second);
        if (pay > 0.005)
            settlements.push_back({debtors[i].first, creditors[j].first, pay});
        debtors[i].second -= pay;
        creditors[j].second -= pay;
        if (debtors[i].second <= 0.005) ++i;
        if (creditors[j].second <= 0.005) ++j;
    }

    stringstream ss;
    ss << "{\"group\":\"" << jsonEscape(gname) << "\",\"settlements\":[";
    bool first = true;
    for (auto &t : settlements) {
        if (!first) ss << ",";
        ss << "{\"from\":\"" << get<0>(t) << "\",\"to\":\"" << get<1>(t)
           << "\",\"amount\":" << formatAmount(get<2>(t)) << "}";
        first = false;
    }
    ss << "]}";
    return makeJson(ss.str());
}
