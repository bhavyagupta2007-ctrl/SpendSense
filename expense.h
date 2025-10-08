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
    std::vector<double> shares; // per-member share (sums to amount)
    std::string date;
};

struct Group {
    std::string name;
    std::vector<std::string> members;
    std::vector<Expense> expenses;
};

#endif // EXPENSE_H