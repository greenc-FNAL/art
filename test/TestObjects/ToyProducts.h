#ifndef test_TestObjects_ToyProducts_h
#define test_TestObjects_ToyProducts_h

// ======================================================================
//
// EDProducts for testing purposes
//
// ======================================================================

#include "art/Persistency/Common/traits.h"
#include "cetlib/container_algorithms.h"
#include "cpp0x/cstdint"
#include <stdexcept>
#include <string>
#include <vector>

namespace arttest
{

  struct DummyProduct
  { };

  struct IntProduct
  {
    explicit IntProduct(int i=0) : value(i) { }
    ~IntProduct() { }

    int value;
  };

  struct Int16_tProduct
  {
    explicit Int16_tProduct(int16_t i=0, uint16_t j=1) :value(i), uvalue(j) {}
    ~Int16_tProduct() {}
    int16_t value;
    uint16_t uvalue;
  };

  struct DoubleProduct
  {
    explicit DoubleProduct(double d=2.2) : value(d) { }
    ~DoubleProduct() { }

    double value;
  };

  struct StringProduct
  {
    StringProduct() : name_() {}
    explicit StringProduct(const std::string& s) : name_(s){}
    std::string name_;
  };

  struct Simple
  {
    Simple() : key(0), value(0.0) { }
    virtual ~Simple() { }
    typedef int key_type;

    key_type    key;
    double      value;

    key_type id() const { return key; }
    virtual double dummy() const { return -3.14; }
    virtual Simple* clone() const { return new Simple(*this); }
  };

  inline
  bool
  operator== (Simple const& a, Simple const& b)
  {
    return (a.key==b.key && a.value==b.value);
  }

  inline
  bool operator< (Simple const& a, Simple const& b)
  {
    return a.key < b.key;
  }

  struct SimpleDerived : public Simple
  {
    SimpleDerived() : Simple(), dummy_(16.25) { }

    SimpleDerived( SimpleDerived const & other)
      : Simple(other), dummy_(other.dummy_)
    { }

    double dummy_;
    double dummy() const { return dummy_; }
    SimpleDerived* clone() const { return new SimpleDerived(*this); }
  };

  struct Sortable
  {
    int data;
    Sortable() : data(0) { }
    explicit Sortable(int i) : data(i) { }
  };

  inline
  bool
  operator== (Sortable const& a, Sortable const& b)
  {
    return (a.data == b.data);
  }

  inline
  bool operator< (Sortable const& a, Sortable const& b)
  {
    return a.data < b.data;
  }

  struct Unsortable : public art::DoNotSortUponInsertion
  {
    int data;
    Unsortable() : data(0) { }
    explicit Unsortable(int i) : data(i) { }
  };

  inline
  bool operator< (Unsortable const&, Unsortable const&)
  {
    throw std::logic_error("operator< called for Unsortable");
  }

  struct Prodigal : public art::DoNotRecordParents
  {
    int data;
    Prodigal() : data(0) { }
    explicit Prodigal(int i) : data(i) { }
  };

  typedef std::vector<Simple>           VSimpleProduct;
}

// ======================================================================

#endif /* test_TestObjects_ToyProducts_h */

// Local Variables:
// mode: c++
// End:
