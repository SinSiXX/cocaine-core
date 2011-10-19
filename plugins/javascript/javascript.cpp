#include <v8.h>

#include "cocaine/downloads.hpp"
#include "cocaine/plugin.hpp"
#include "cocaine/helpers/uri.hpp"

// Allowed exceptions:
// -------------------
// * std::runtime_error

namespace cocaine { namespace plugin {

using namespace v8;

class javascript_t: public source_t {
    public:
        javascript_t(const std::string& name, const std::string& args):
            source_t(name)
        {
            if(args.empty()) {
                throw std::runtime_error("no code location has been specified");
            }
            
            helpers::uri_t uri(args);
            compile(helpers::download(uri), "iterate");
        }

        void compile(const std::string& code,
                     const std::string& name)
        {
            HandleScope handle_scope;

            m_context = Context::New();

            Context::Scope context_scope(m_context);
            
            TryCatch try_catch;

            Handle<String> source(String::New(code.c_str()));
            Handle<Script> script(Script::Compile(source));

            if(script.IsEmpty()) {
                String::AsciiValue exception(try_catch.Exception());
                throw std::runtime_error(*exception);
            }

            Handle<Value> result(script->Run());

            if(result.IsEmpty()) {
                String::AsciiValue exception(try_catch.Exception());
                throw std::runtime_error(*exception);
            }

            Handle<String> target(String::New(name.c_str()));
            Handle<Value> object(m_context->Global()->Get(target));

            if(!object->IsFunction()) {
                throw std::runtime_error("target object is not a function");
            }

            Handle<Function> function(Handle<Function>::Cast(object));
            m_function = Persistent<Function>::New(function);
        }

        ~javascript_t() {
            m_function.Dispose();
            m_context.Dispose();
        }

        virtual Json::Value invoke(const std::string& callable, const void* request, size_t request_length) {
            Json::Value result;

            HandleScope handle_scope;
            Context::Scope context_scope(m_context);
            
            TryCatch try_catch;
            Handle<Value> rv(m_function->Call(m_context->Global(), 0, NULL));

            if(!rv.IsEmpty()) {
                result["result"] = "success";
            } else if(try_catch.HasCaught()) {
                String::AsciiValue exception(try_catch.Exception());
                result["error"] = std::string(*exception, exception.length());
            }

            return result;
        }

    private:
        Persistent<Context> m_context;
        Persistent<Function> m_function;
};

source_t* create_javascript_instance(const char* name, const char* args) {
    return new javascript_t(name, args);
}

static const source_info_t plugin_info[] = {
    { "javascript", &create_javascript_instance },
    { NULL, NULL }
};

extern "C" {
    const source_info_t* initialize() {
        // Global initialization logic
        // This function will be called once, from the main thread

        return plugin_info;
    }

    // __attribute__((destructor)) void finalize() {
        // This is guaranteed to be called from the main thread,
        // when there're no more plugin instances left running
    // }
}

}}
