#include <gtest/gtest.h>

#include <DataTypes/DataTypesNumber.h>
#include <Databases/DatabaseMemory.h>
#include <Storages/StorageMemory.h>
#include <Parsers/ASTCreateQuery.h>
#include <Common/tests/gtest_global_context.h>
#include "Storages/MemorySettings.h"

using namespace DB;

class DatabaseMemoryTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Create a DatabaseMemory instance
        const auto & context_holder = getContext();
        context = context_holder.context;
        database = std::make_shared<DatabaseMemory>("test_db", context);
    }

    void TearDown() override
    {
        // Clean up
        if (database)
        {
            database->shutdown();
        }
    }

    StoragePtr createTestTable(const String & table_name)
    {
        // Create a simple Memory storage
        NamesAndTypesList columns;
        columns.emplace_back("id", std::make_shared<DataTypeUInt64>());
        
        auto storage = std::make_shared<StorageMemory>(
            StorageID("test_db", table_name),
            ColumnsDescription(columns),
            ConstraintsDescription{},
            String{},
            MemorySettings{});
        
        return storage;
    }

    ContextPtr context;
    std::shared_ptr<DatabaseMemory> database;
};

// For Motivation: refer to https://github.com/ClickHouse/ClickHouse/issues/88615
TEST_F(DatabaseMemoryTest, DropTableCleanup)
{
    const String table_name = "test_table";
    
    // Create and add a table to the database
    auto storage = createTestTable(table_name);
    auto create_query = std::make_shared<ASTCreateQuery>();
    create_query->setDatabase("test_db");
    create_query->setTable(table_name);
    
    database->createTable(context, table_name, storage, create_query);
    
    // Verify the table exists
    EXPECT_TRUE(database->isTableExist(table_name, context));
    
    // Verify create query is stored
    auto stored_query = database->tryGetCreateTableQuery(table_name, context);
    EXPECT_NE(stored_query, nullptr) << "Create query should exist before drop";
    
    // Drop the table
    database->dropTable(context, table_name, /* sync */ false);
    
    // Verify the table is dropped
    EXPECT_FALSE(database->isTableExist(table_name, context));
    
    // Verify create_queries was cleaned up
    auto query_after_drop = database->tryGetCreateTableQuery(table_name, context);
    EXPECT_EQ(query_after_drop, nullptr) << "Create query should be removed after drop";
    
    // Verify snapshot_detached_tables doesn't contain the entry
    auto detached_iterator = database->getDetachedTablesIterator(context, {}, false);
    
    bool found = false;
    while (detached_iterator->isValid())
    {
        if (detached_iterator->table() == table_name)
        {
            found = true;
            break;
        }
        detached_iterator->next();
    }
    EXPECT_FALSE(found) << "Table " << table_name << " should not be in snapshot_detached_tables after drop";
}
