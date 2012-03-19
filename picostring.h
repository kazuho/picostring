#ifndef picostring_h
#define picostring_h

#include <algorithm>
#include <cassert>

template <typename StringT> class picostring {
public:
  typedef typename StringT::value_type char_type;
  typedef typename StringT::size_type size_type;
private:
  
  class SimpleS;
  
  struct BaseS {
    mutable size_t refcnt_;
    BaseS() : refcnt_(0) {}
    virtual ~BaseS() {}
    const BaseS* retain() const { refcnt_++; return this; }
    void release() const {
      if (refcnt_-- == 0) delete const_cast<BaseS*>(this);
    }
    virtual size_type size() const = 0;
    virtual char_type at(size_type pos) const = 0;
    virtual const BaseS* substr(size_type pos, size_type length) const = 0;
    virtual const BaseS* append(const BaseS* s) const = 0;
    virtual const BaseS* append(const StringT& s) const = 0;
    virtual const SimpleS* flatten() const = 0;
    virtual char_type* flatten(char_type* out) const = 0;
  };
  
  struct SimpleS : public BaseS {
    const StringT s_;
    const size_type first_;
    const size_type last_;
    SimpleS(const StringT& s, size_type first, size_type last)
      : s_(s), first_(first), last_(last) {}
    virtual size_type size() const {
      return s_.size();
    }
    virtual char_type at(size_type pos) const {
      return s_[first_ + pos];
    }
    virtual const BaseS* substr(size_type pos, size_type length) const {
      return new SimpleS(s_, first_ + pos, first_ + pos + length);
    }
    virtual const BaseS* append(const BaseS* s) const {
      return new LinkS(this->retain(), s->retain());
    }
    virtual const BaseS* append(const StringT& s) const {
      return new LinkS(this->retain(), new SimpleS(s, 0, s.size()));
    }
    virtual const SimpleS* flatten() const {
      if (first_ == 0 && last_ == s_.size())
	return this;
      size_t length = last_ - first_;
      return new SimpleS(s_.substr(first_, length), 0, length);
    }
    virtual char_type* flatten(char_type* out) const {
      std::copy(s_.begin() + first_, s_.begin() + last_, out);
      return out + last_ - first_;
    }
  };
  
  struct LinkS : public BaseS {
    const BaseS* left_;
    const BaseS* right_;
    size_type size_;
  public:
    LinkS(const BaseS* left, const BaseS* right)
      : left_(left), right_(right), size_(left->size() + right->size()) {}
    ~LinkS() {
      left_->release();
      right_->release();
    }
    virtual size_type size() const {
      return size_;
    }
    virtual char_type at(size_type pos) const {
      return pos < left_->size()
	? left_->at(pos) : right_->at(pos - left_->size());
    }
    virtual const BaseS* substr(size_type pos, size_type length) const {
      if (pos < left_->size()) {
	if (pos + length <= left_->size()) {
	  return left_->substr(pos, length);
	} else {
	  return new LinkS(left_->substr(pos, left_->size() - pos),
			   right_->substr(0, pos + length - left_->size()));
	}
      } else {
	return right_->substr(pos - left_->size(), length);
      }
    }
    virtual const BaseS* append(const BaseS* s) const {
      return new LinkS(this->retain(), s->retain());
    }
    virtual const BaseS* append(const StringT& s) const {
      return new LinkS(this->retain(), new SimpleS(s, 0, s.size()));
    }
    virtual const SimpleS* flatten() const {
      StringT s(this->size(), char_type());
      flatten(&s[0]);
      return new SimpleS(s, 0, this->size());
    }
    virtual char_type* flatten(char_type* out) const {
      out = left_->flatten(out);
      out = right_->flatten(out);
      return out;
    }
  };
  
  const BaseS* s_;
  
  explicit picostring(const BaseS* s) : s_(s) {}
public:
  picostring() : s_(NULL) {}
  picostring(const picostring& s) : s_(s.s_->retain()) {}
  picostring(const StringT& s) : s_(NULL) {
    if (! s.empty()) s_ = new SimpleS(s, 0, s.size());
  }
  picostring& operator=(const picostring& s) {
    if (this != &s) {
      if (s_ != NULL) s_->release();
      s_ = s.s_ != NULL ? s.s_->retain() : NULL;
    }
    return *this;
  }
  ~picostring() {
    if (s_ != NULL) s_->release();
  }
  bool empty() const { return s_ == NULL; }
  size_type size() const { return s_ != NULL ? s_->size() : 0; }
  char_type at(size_type pos) const {
    assert(s_ != NULL);
    assert(pos < s_->size());
    return s_->at(pos);
  }
  picostring substr(size_type pos, size_type length) const {
    assert(pos + length <= s_->size());
    if (length == 0)
      return picostring();
    assert(s_ != NULL);
    return picostring(s_->substr(pos, length));
  }
  picostring append(const picostring& s) const {
    if (s_ == NULL)
      return s;
    return picostring(s_->append(s));
  }
  picostring append(const StringT& s) const {
    if (s.empty())
      return *this;
    else if (s_ == NULL)
      return picostring(s);
    else
      return picostring(s_->append(s));
  }
  const StringT& str() const {
    static StringT emptyStr;
    if (s_ == NULL)
      return emptyStr;
    const SimpleS* flat = s_->flatten();
    if (flat != s_) {
      s_->release();
      const_cast<picostring*>(this)->s_ = flat;
    }
    return flat->s_;
  }
};

#ifdef TEST_PICOSTRING

#include <cstdio>
#include <string>

using namespace std;

static void plan(int num)
{
  printf("1..%d\n", num);
}

static bool success = true;

static void ok(bool b, const char* name = "")
{
  static int n = 1;
  if (! b)
    success = false;
  printf("%s %d - %s\n", b ? "ok" : "ng", n++, name);
}

template <typename T> void is(const T& x, const T& y, const char* name = "")
{
  if (x == y) {
    ok(true, name);
  } else {
    ok(false, name);
  }
}

typedef picostring<string> picostr;

int main(int, char**)
{
  plan(45);
  
  is(picostr().str(), string());
  ok(picostr().empty());
  is(picostr().size(), (picostr::size_type)0);

  is(picostr("").str(), string());
  ok(picostr("").empty());
  is(picostr("").size(), (picostr::size_type)0);
  
  is(picostr("").append("abc").str(), string("abc"));
  is(picostr("abc").append("de").str(), string("abcde"));
  is(picostr("abc").append("de").append("f").str(), string("abcdef"));

  picostr s = picostr("abc").append("de").append("f");
  ok(! s.empty());
  is(s.size(), (picostr::size_type)6);
  is(s.at(0), 'a');
  is(s.at(1), 'b');
  is(s.at(2), 'c');
  is(s.at(3), 'd');
  is(s.at(4), 'e');
  is(s.at(5), 'f');
  
  is(s.substr(0, 6).str(), string("abcdef"));
  is(s.substr(0, 5).str(), string("abcde"));
  is(s.substr(0, 4).str(), string("abcd"));
  is(s.substr(0, 3).str(), string("abc"));
  is(s.substr(0, 2).str(), string("ab"));
  is(s.substr(0, 1).str(), string("a"));
  is(s.substr(0, 0).str(), string(""));
  is(s.substr(1, 5).str(), string("bcdef"));
  is(s.substr(1, 4).str(), string("bcde"));
  is(s.substr(1, 3).str(), string("bcd"));
  is(s.substr(1, 2).str(), string("bc"));
  is(s.substr(1, 1).str(), string("b"));
  is(s.substr(1, 0).str(), string(""));
  is(s.substr(2, 4).str(), string("cdef"));
  is(s.substr(2, 3).str(), string("cde"));
  is(s.substr(2, 2).str(), string("cd"));
  is(s.substr(2, 1).str(), string("c"));
  is(s.substr(2, 0).str(), string(""));
  is(s.substr(3, 3).str(), string("def"));
  is(s.substr(3, 2).str(), string("de"));
  is(s.substr(3, 1).str(), string("d"));
  is(s.substr(3, 0).str(), string(""));
  is(s.substr(4, 2).str(), string("ef"));
  is(s.substr(4, 1).str(), string("e"));
  is(s.substr(4, 0).str(), string(""));
  is(s.substr(5, 1).str(), string("f"));
  is(s.substr(5, 0).str(), string(""));
  is(s.substr(6, 0).str(), string(""));
  
  return 0;
}

#endif

#endif
