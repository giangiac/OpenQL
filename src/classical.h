/**
 * @file   classical.h
 * @date   05/2018
 * @author Imran Ashraf
 * @brief  classical operation implementation
 */

#ifndef _CLASSICAL_H
#define _CLASSICAL_H

#include <fstream>
#include <iomanip>
#include <complex>

#include <string>
#include <sstream>
#include <map>
#include <stack>

#include <compile_options.h>
#include <utils.h>
#include <str.h>
#include <gate.h>
#include <exception.h>


namespace ql
{

enum class operation_type_t
{
    ARITHMATIC, RELATIONAL, BITWISE
};
enum class operand_type_t
{
    CREG, CVAL
};

class coperand
{
public:
    size_t id;
    int value;
    virtual ql::operand_type_t type() = 0;
    virtual void print() = 0;
    virtual ~coperand(){}
};


class cval: public coperand
{
public:
    cval(int val)
    {
        value=val;
    }
    cval(const cval &cv)
    {
        value = cv.value;
    }
    ql::operand_type_t type() { return operand_type_t::CVAL;}
    void print()
    {
        COUT("cval with value: " << value);
    }
    ~cval() {}
};

class creg: public coperand
{
public:
    creg(size_t id)
    {
        this->id = id;
        DOUT("creg constructor, used id: " << id);
    }

    creg(const creg &c)
    {
        id = c.id;
        DOUT("creg copy constructor, used id: " << id);
    }

    ql::operand_type_t type() { return operand_type_t::CREG;}

    void print()
    {
        COUT("creg with id: " << id);
    }

    ~creg()
    {
    }
};

class operation
{
public:
    std::string operation_name;
    std::string inv_operation_name;
    operation_type_t operation_type;
    std::vector<coperand*> operands;

    operation() {}
    operation(creg& l, std::string op, creg& r)
    {
        operands.push_back(new ql::creg(l));
        operands.push_back(new ql::creg(r));
        if(op == "+")
        {
            operation_name = "add";
            operation_type = ql::operation_type_t::ARITHMATIC;
        }
        else if(op == "-")
        {
            operation_name = "sub";
            operation_type = ql::operation_type_t::ARITHMATIC;
        }
        else if(op == "&")
        {
            operation_name = "and";
            operation_type = ql::operation_type_t::BITWISE;
        }
        else if(op == "|")
        {
            operation_name = "or";
            operation_type = ql::operation_type_t::BITWISE;
        }
        else if(op == "^")
        {
            operation_name = "xor";
            operation_type = ql::operation_type_t::BITWISE;
        }
        else if(op == "==")
        {
            operation_name = "eq";
            inv_operation_name = "ne";
            operation_type = ql::operation_type_t::RELATIONAL;
        }
        else if(op == "!=")
        {
            operation_name = "ne";
            inv_operation_name = "eq";
            operation_type = ql::operation_type_t::RELATIONAL;
        }
        else if(op == "<")
        {
            operation_name = "lt";
            inv_operation_name = "ge";
            operation_type = ql::operation_type_t::RELATIONAL;
        }
        else if(op == ">")
        {
            operation_name = "gt";
            inv_operation_name = "le";
            operation_type = ql::operation_type_t::RELATIONAL;
        }
        else if(op == "<=")
        {
            operation_name = "le";
            inv_operation_name = "gt";
            operation_type = ql::operation_type_t::RELATIONAL;
        }
        else if(op == ">=")
        {
            operation_name = "ge";
            inv_operation_name = "lt";
            operation_type = ql::operation_type_t::RELATIONAL;
        }
        else
        {
            EOUT("Unknown binary operation '" << op );
            throw ql::exception("Unknown binary operation '"+op+"' !", false);
        }
    }

    // used for assign
    operation(creg& l)
    {
        operation_name = "mov";
        operation_type = ql::operation_type_t::ARITHMATIC;
        operands.push_back(new ql::creg(l));
    }

    // used for initializing with an imm
    operation(cval & v)
    {
        operation_name = "ldi";
        operation_type = ql::operation_type_t::ARITHMATIC;
        operands.push_back(new ql::cval(v));
    }

    // used for initializing with an imm
    operation(int val)
    {
        operation_name = "ldi";
        operation_type = ql::operation_type_t::ARITHMATIC;
        operands.push_back(new ql::cval(val));
    }

    operation(std::string op, creg& r)
    {
        if(op == "~")
        {
            operation_name = "not";
            operation_type = ql::operation_type_t::BITWISE;
            operands.push_back(new ql::creg(r));
        }
        else
        {
            EOUT("Unknown unary operation '" << op );
            throw ql::exception("Unknown unary operation '"+op+"' !", false);
        }
    }
};


class classical : public gate
{
public:
    // int imm_value;
    cmat_t m;

    classical(creg& dest, operation & oper)
    {
        DOUT("Classical gate constructor with destination for " << oper.operation_name);
        name = oper.operation_name;
        duration = 20;
        creg_operands.push_back(dest.id);
        if(name == "ldi")
        {
            int_operand = (oper.operands[0])->value;
            DOUT("... setting int_operand of " << oper.operation_name << " to " << int_operand);
        }
        else
        {
            for(auto & op : oper.operands)
            {
                creg_operands.push_back(op->id);
            }
        }
    }

    classical(std::string operation)
    {
        DOUT("Classical gate constructor for " << operation);
        str::lower_case(operation);
        if((operation == "nop"))
        {
            name=operation;
            duration = 20;
            DOUT("Adding 0 operand operation: " << name);
        }
        else
        {
            EOUT("Unknown classical operation '" << name << "' with '0' operands!");
            throw ql::exception("Unknown classical operation'"+name+"' with'0' operands!", false);
        }
    }

    instruction_t qasm()
    {
        std::string iopers;
        int sz = creg_operands.size();
        for(int i=0; i<sz; ++i)
        {
            if(i==sz-1)
                iopers += " r" + std::to_string(creg_operands[i]);
            else
                iopers += " r" + std::to_string(creg_operands[i]) + ",";
        }

        if(name == "ldi")
        {
            return "ldi" + iopers + ", " + std::to_string(int_operand);
        }
        else
            return name + iopers;
    }

    gate_type_t type()
    {
        return __classical_gate__;
    }

    cmat_t mat()
    {
        return m;
    }

};


}

#endif // _CLASSICAL_H
