#pragma once

#include <AggregateFunctions/IAggregateFunction.h>
#include <Columns/ColumnAggregateFunction.h>
#include <DataTypes/DataTypeAggregateFunction.h>
#include <Common/assert_cast.h>
#include <Common/typeid_cast.h>


namespace DB
{
namespace ErrorCodes
{
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
}
}

namespace local_engine
{
using namespace DB;

struct Settings;

/**
 * this class is copied from AggregateFunctionMerge with little enhancement.
 * we use this PartialMerge for both spark PartialMerge and Final
 */


class AggregateFunctionPartialMerge final : public IAggregateFunctionHelper<AggregateFunctionPartialMerge>
{
private:
    AggregateFunctionPtr nested_func;

public:
    AggregateFunctionPartialMerge(const AggregateFunctionPtr & nested_, const DataTypePtr & argument, const Array & params_)
        : IAggregateFunctionHelper<AggregateFunctionPartialMerge>({argument}, params_, createResultType(nested_)), nested_func(nested_)
    {
        const DataTypeAggregateFunction * data_type = typeid_cast<const DataTypeAggregateFunction *>(argument.get());

        if (!data_type || !nested_func->haveSameStateRepresentation(*data_type->getFunction()))
            throw Exception(
                ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT,
                "Illegal type {} of argument for aggregate function {}, "
                "expected {} or equivalent type",
                argument->getName(),
                getName(),
                getStateType()->getName());
    }

    String getName() const override { return nested_func->getName() + "PartialMerge"; }

    static DataTypePtr createResultType(const AggregateFunctionPtr & nested_) { return nested_->getResultType(); }

    const DataTypePtr & getResultType() const override { return nested_func->getResultType(); }

    bool isVersioned() const override { return nested_func->isVersioned(); }

    size_t getDefaultVersion() const override { return nested_func->getDefaultVersion(); }

    DataTypePtr getStateType() const override { return nested_func->getStateType(); }

    void create(AggregateDataPtr __restrict place) const override { nested_func->create(place); }

    void destroy(AggregateDataPtr __restrict place) const noexcept override { nested_func->destroy(place); }

    bool hasTrivialDestructor() const override { return nested_func->hasTrivialDestructor(); }

    size_t sizeOfData() const override { return nested_func->sizeOfData(); }

    size_t alignOfData() const override { return nested_func->alignOfData(); }

    void add(AggregateDataPtr __restrict place, const IColumn ** columns, size_t row_num, Arena * arena) const override
    {
        nested_func->merge(place, assert_cast<const ColumnAggregateFunction &>(*columns[0]).getData()[row_num], arena);
    }

    void merge(AggregateDataPtr __restrict place, ConstAggregateDataPtr rhs, Arena * arena) const override
    {
        nested_func->merge(place, rhs, arena);
    }

    void serialize(ConstAggregateDataPtr __restrict place, WriteBuffer & buf, std::optional<size_t> version) const override
    {
        nested_func->serialize(place, buf, version);
    }

    void deserialize(AggregateDataPtr __restrict place, ReadBuffer & buf, std::optional<size_t> version, Arena * arena) const override
    {
        nested_func->deserialize(place, buf, version, arena);
    }

    void insertResultInto(AggregateDataPtr __restrict place, IColumn & to, Arena * arena) const override
    {
        nested_func->insertResultInto(place, to, arena);
    }

    bool allocatesMemoryInArena() const override { return nested_func->allocatesMemoryInArena(); }

    AggregateFunctionPtr getNestedFunction() const override { return nested_func; }
};

}
