#ifndef DBALLE_CMDLINE_PROCESSOR_H
#define DBALLE_CMDLINE_PROCESSOR_H

#include <dballe/msg/codec.h>
#include <stdexcept>
#include <list>
#include <string>

namespace wreport {
struct Bulletin;
}

namespace dballe {
struct Query;
struct BinaryMessage;
struct Msgs;
struct Matcher;

namespace cmdline {

/**
 * Exception used to embed processing issues that mean that processing of the
 * current element can safely be skipped.
 *
 * When this exception is caught we know, for example, that no output has been
 * produced for the item currently being processed.
 */
struct ProcessingException : public std::exception
{
    std::string msg;

    /**
     * Create a new exception
     *
     * @param filename Input file being processed
     * @param index Index of the data being processed in the input file
     * @param msg Error message
     * @param original (optional) original exception that was caught from the
     *        underlying subsystem
     */
    ProcessingException(
            const std::string& filename,
            unsigned index,
            const std::string& msg)
    {
        initmsg(filename, index, msg.c_str());
    }

    ProcessingException(
            const std::string& filename,
            unsigned index,
            const std::exception& original)
    {
        initmsg(filename, index, original.what());
    }

    ProcessingException(
            const std::string& filename,
            unsigned index,
            const std::string& msg,
            const std::exception& original)
    {
        initmsg(filename, index, msg.c_str());
        this->msg += ": ";
        this->msg += original.what();
    }

    virtual ~ProcessingException() throw() {}

    virtual const char* what() const throw ()
    {
        return msg.c_str();
    }

protected:
    void initmsg(const std::string& fname, unsigned index, const char* msg);
};

struct Item
{
    unsigned idx;
    BinaryMessage* rmsg;
    wreport::Bulletin* bulletin;
    Msgs* msgs;

    Item();
    ~Item();

    /// Decode all that can be decoded
    void decode(msg::Importer& imp, bool print_errors=false);

    /// Set the value of msgs, possibly replacing the previous one
    void set_msgs(Msgs* new_msgs);
};

struct Action
{
    virtual ~Action() {}
    virtual bool operator()(const Item& item) = 0;
};

struct Filter
{
    msg::Exporter::Options export_opts;
    int category;
    int subcategory;
    int checkdigit;
    int unparsable;
    int parsable;
    const char* index;
    Matcher* matcher;

    Filter();
    ~Filter();

    /// Reset to the empty matcher
    void matcher_reset();

    /// Initialise the matcher from a record
    void matcher_from_record(const Query& query);

    bool match_index(int idx) const;
    bool match_common(const BinaryMessage& rmsg, const Msgs* msgs) const;
    bool match_msgs(const Msgs& msgs) const;
    bool match_bufrex(const BinaryMessage& rmsg, const wreport::Bulletin* rm, const Msgs* msgs) const;
    bool match_bufr(const BinaryMessage& rmsg, const wreport::Bulletin* rm, const Msgs* msgs) const;
    bool match_crex(const BinaryMessage& rmsg, const wreport::Bulletin* rm, const Msgs* msgs) const;
    bool match_aof(const BinaryMessage& rmsg, const Msgs* msgs) const;
    bool match_item(const Item& item) const;
};

class Reader
{
protected:
    void read_csv(const std::list<std::string>& fnames, Action& action);
    void read_file(const std::list<std::string>& fnames, Action& action);

public:
    const char* input_type;
    msg::Importer::Options import_opts;
    Filter filter;
    bool verbose;
    const char* fail_file_name;

    Reader();

    void read(const std::list<std::string>& fnames, Action& action);
};

}
}
#endif
