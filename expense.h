#ifndef EXPENSE_H
#define EXPENSE_H

#include <string>
#include <vector>

struct Expense {
    std::string id;
    std::string name;
    std::string category;
    double amount;
    std::string payer;
    std::vector<std::string> members;
    std::vector<double> shares;
    std::string date;
};

struct Group {
    std::string name;
    std::vector<std::string> members;
    std::vector<Expense> expenses;
};

#ifdef __cplusplus
extern "C" {
#endif

const char* createGroup(const char* groupName, const char* members_str);
const char* listGroups();
const char* getGroupMembers(const char* groupName);
const char* addGroupExpense(const char* groupName, const char* name, const char* category,
                            double amount, const char* payer, const char* members_str,
                            const char* shares_str, const char* date);
const char* editExpense(const char* groupName, const char* expenseId,
                        const char* name, const char* category, double amount,
                        const char* payer, const char* members_str,
                        const char* shares_str, const char* date);
const char* deleteExpense(const char* groupName, const char* expenseId);
const char* showGroupExpenses(const char* groupName);
const char* calculateGroupSettlement(const char* groupName);

#ifdef __cplusplus
}
#endif

#endif
