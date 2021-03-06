
#include "morpheus/Utils.hpp"

#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

#include <sstream>

namespace Utils {

template<typename T>
void store_if_not(std::vector<T> &values, const T &v, T excl) {
  if (v != excl) {
    values.push_back(v);
  }
}

template<typename T>
std::string pp_vector(const std::vector<T> &values,
                      std::string delim,
                      std::string lbracket,
                      std::string rbracket,
                      std::function<bool(const T &)> filter_fn) {

  std::vector<T const *> values_;
  for (const T &v : values) {
    if (filter_fn(v)) {
      values_.push_back(&v);
    }
  }

  std::ostringstream oss;
  if (values_.empty()) {
    return "";
  }

  if (values_.size() == 1) {
    oss << *values_[0];
    return oss.str();
  }

  // more than one value
  oss << lbracket;
  auto it = values_.begin();
  oss << **it; it++;
  for (; it != values_.end(); it++) {
    oss << delim << **it;
  }
  oss << rbracket;
  return oss.str();
}

std::string value_to_type(const llvm::Value &v, bool return_constant) {
  if (llvm::isa<llvm::Constant>(v) && !return_constant) {
    // NOTE: for constant value the type is represented by empty string
    //       because the constants are used directly without need to store them.
    return "";
  }
  std::string type;
  llvm::raw_string_ostream rso(type);
  rso << *v.getType();
  return rso.str();
}

std::string value_to_str(const llvm::Value &v, std::string name, bool return_constant) {
  if (llvm::Constant const *c = llvm::dyn_cast<llvm::Constant>(&v)) {
    if (return_constant) {
      std::string value;
      llvm::raw_string_ostream rso(value);
      v.printAsOperand(rso, false); // print the constant without the type
      return rso.str();
    }
    return "";
  }
  return name;
}

std::string compute_envelope_type(llvm::Value const *src,
                                  llvm::Value const *dest,
                                  const llvm::Value &tag,
                                  std::string delim,
                                  std::string lbracket,
                                  std::string rbracket) {

  using namespace std::placeholders;
  auto store_non_empty = std::bind(store_if_not<std::string>, _1, _2, "");

  std::vector<std::string> types;

  if (src) {
    store_non_empty(types, value_to_type(*src, false));
  }
  if (dest) {
    store_non_empty(types, value_to_type(*dest, false));
  }
  store_non_empty(types, value_to_type(tag, false));

  return pp_vector(types, delim, lbracket, rbracket);
}

std::string compute_envelope_value(llvm::Value const *src,
                                   llvm::Value const *dest,
                                   const llvm::Value &tag,
                                   bool include_constant,
                                   std::string delim,
                                   std::string lbracket,
                                   std::string rbracket) {

  using namespace std::placeholders;
  auto store_non_empty = std::bind(store_if_not<std::string>, _1, _2, "");

  std::vector<std::string> var_names;
  if (src) {
    std::string src_ = value_to_str(*src, "src", include_constant);
    if (src_ == "-2") { // MPI_ANY_SOURCE
      src_ = "";
    }
    store_non_empty(var_names, src_);
  }

  if (dest) {
    store_non_empty(var_names, value_to_str(*dest, "dest", include_constant));
  }

  store_non_empty(var_names, value_to_str(tag, "tag", include_constant));

  return pp_vector(var_names, delim, lbracket, rbracket);
}

std::string compute_data_buffer_type(const llvm::Value &) {

  return "DataToken"; // NOTE: it may possibly returns a record type:
                       //       {"type": T, "size": int}
}

std::string compute_data_buffer_value(const llvm::Value &,
                                      const llvm::Value &) {

  return "data"; // NOTE: it may possibly return a record:
                        //       {"data": buff, "type": a_type, "size": a_size}
}

std::string compute_msg_rqst_value(llvm::Value const *src,
                                   llvm::Value const *dest,
                                   const llvm::Value &tag,
                                   std::string buffered,
                                   std::string delim,
                                   std::string lbracket,
                                   std::string rbracket) {

  auto prepare_part = [] (std::string name,
                          const std::string &val) -> std::string {
    if (val.empty()) {
      return "";
    }

    std::ostringstream oss;
    oss << name << "=" << val;
    return oss.str();
  };

  using namespace std::placeholders;
  auto store_non_empty = std::bind(store_if_not<std::string>, _1, _2, "");

  std::vector<std::string> parts;
  parts.push_back("id=unique(id)");
  if (src) {
    std::string src_ = value_to_str(*src, "src");
    if (src_ == "-2") { // MPI_ANY_SOURCE
      src_ = "";
    }
    store_non_empty(parts, prepare_part("src", src_));
  }

  if (dest) {
    store_non_empty(parts, prepare_part("dest", value_to_str(*dest, "dest")));
  }

  std::string tag_ = value_to_str(tag, "tag");
  if (tag_ == "-1") { // MPI_ANY_TAG
    tag_ = "";
  }
  store_non_empty(parts, prepare_part("tag", tag_));
  store_non_empty(parts, prepare_part("buffered", buffered));
  return pp_vector(parts, "," + delim, lbracket, rbracket);
}

}
