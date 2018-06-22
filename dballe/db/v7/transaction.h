#ifndef DBALLE_DB_V7_TRANSACTION_H
#define DBALLE_DB_V7_TRANSACTION_H

#include <dballe/transaction.h>
#include <dballe/db/db.h>
#include <dballe/db/v7/fwd.h>
#include <dballe/db/v7/data.h>
#include <dballe/db/v7/batch.h>
#include <memory>

namespace dballe {
namespace db {
namespace v7 {

struct Transaction : public dballe::db::Transaction
{
protected:
    /// Report information
    v7::Repinfo* m_repinfo = nullptr;
    /// Station information
    v7::Station* m_station = nullptr;
    /// Level/timerange information
    v7::LevTr* m_levtr = nullptr;
    /// Station data
    v7::StationData* m_station_data = nullptr;
    /// Variable data
    v7::Data* m_data = nullptr;

    void add_msg_to_batch(const Message& message, const char* repmemo, int flags);
    void print_profile_counters();

public:
    typedef v7::DB DB;

    std::shared_ptr<v7::DB> db;
    /// SQL-side transaction
    dballe::Transaction* sql_transaction = nullptr;
    /// True if commit or rollback have already been called on this transaction
    bool fired = false;
    /// Batch importer
    v7::Batch batch;
    /// Query-level tracer
    v7::SQLTrace* trace = nullptr;
    /// Tracing system
    v7::Tracer<v7::trace::Transaction> trc;

    Transaction(std::shared_ptr<v7::DB> db, std::unique_ptr<dballe::Transaction> sql_transaction);
    Transaction(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction& operator=(Transaction&&) = delete;
    ~Transaction();

    void reset_profile_counters();

    /// Access the repinfo table
    v7::Repinfo& repinfo();
    /// Access the station table
    v7::Station& station();
    /// Access the levtr table
    v7::LevTr& levtr();
    /// Access the station_data table
    v7::StationData& station_data();
    /// Access the data table
    v7::Data& data();

    void commit() override;
    void rollback() override;
    void clear_cached_state() override;

    std::unique_ptr<db::CursorStation> query_stations(const Query& query);
    std::unique_ptr<db::CursorStationData> query_station_data(const Query& query) override;
    std::unique_ptr<db::CursorData> query_data(const Query& query);
    std::unique_ptr<db::CursorSummary> query_summary(const Query& query);
    void attr_query_station(int data_id, std::function<void(std::unique_ptr<wreport::Var>)>&& dest) override;
    void attr_query_data(int data_id, std::function<void(std::unique_ptr<wreport::Var>)>&& dest) override;

    void insert_station_data(StationValues& vals, bool can_replace, bool station_can_add) override;
    void insert_data(DataValues& vals, bool can_replace, bool station_can_add) override;
    void remove_station_data(const Query& query) override;
    void remove(const Query& query);
    void remove_all() override;

    void attr_insert_station(int data_id, const Values& attrs) override;
    void attr_insert_data(int data_id, const Values& attrs) override;
    void attr_remove_station(int data_id, const db::AttrList& attrs) override;
    void attr_remove_data(int data_id, const db::AttrList& attrs) override;
    void import_msg(const Message& msg, const char* repmemo, int flags) override;
    void import_msgs(const Messages& msgs, const char* repmemo, int flags) override;
    bool export_msgs(const Query& query, std::function<bool(std::unique_ptr<Message>&&)> dest) override;
    void update_repinfo(const char* repinfo_file, int* added, int* deleted, int* updated) override;

    static Transaction& downcast(dballe::Transaction& transaction);

    void dump(FILE* out) override;
};

struct TestTransaction : public Transaction
{
    using Transaction::Transaction;

    void commit() override;
};

}
}
}
#endif
