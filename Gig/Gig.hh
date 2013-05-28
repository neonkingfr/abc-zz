//_________________________________________________________________________________________________
//|                                                                                      -- INFO --
//| Name        : Gig.hh
//| Author(s)   : Niklas Een
//| Module      : Netlist
//| Description : Second version of the generic netlist datatype.
//| 
//| (C) Copyright 2010-2012, The Regents of the University of California
//|________________________________________________________________________________________________
//|                                                                                  -- COMMENTS --
//| 
//|________________________________________________________________________________________________

#ifndef ZZ__Gig__Gig_hh
#define ZZ__Gig__Gig_hh

#include "ZZ/Generics/Lit.hh"
#include "ZZ/Generics/IdRepos.hh"
#include "BasicTypes.hh"
#include "GateTypes.hh"
#include "GigObjs.hh"

#define ZZ_GIG_PAGED


namespace ZZ {
using namespace std;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Forward declarations:


struct Gig;
struct GigObj;
struct GigLis;


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gate:


struct Gate {
    union {
        uint  inl[3];
        uint* ext;
    };
    uint    type   : 6;
    uint    is_ext : 1;
    uint    size   : 25;

    enum { MAX_TYPE = (1u <<  6) - 1 };
    enum { MAX_SIZE = (1u << 25) - 1 };
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// GigMsg -- messages used to track netlist changes:


enum GigMsg {
    // Generated by netlist operations:
    msg_Update  = 1,    // -- a fanin has changed 
    msg_Add     = 2,    // -- a gates has been added (called before children are connected and attribute is set (attribute changes cannot be listened to))
    msg_Remove  = 4,    // -- a gate is about to be removed (children are NOT explicitly disconnected; this message is the only signal of the fanout change)
    msg_Compact = 8,    // -- netlist was compacted (IDs have changed)

    // Generated by user:
    msg_Subst   = 16,   // -- fanouts of a gate was transferred to an equivalent gate

    msg_All     = 31
};


enum GigMsgIdx {
    msgidx_Update,
    msgidx_Add,
    msgidx_Remove,
    msgidx_Compact,
    msgidx_Subst,
    GigMsgIdx_size
};

extern cchar* GigMsgIdx_name[GigMsgIdx_size];
template<> fts_macro void write_(Out& out, const GigMsgIdx& v) { out += GigMsgIdx_name[v]; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// GigMode:


enum GigMode {
    gig_FreeForm,   // Any gate type can be used 
    gig_Aig,        // Seq, CI, CO, And
    gig_Xig,        // Seq, CI, CO, And, Xor, Mux, Maj
    gig_Npn4,       // Seq, CI, CO, Npn4
    gig_Lut4,       // Seq, CI, CO, Lut4 (which must not have negated inputs)
    gig_Lut6,       // Seq, CI, CO, Lut6 (which must not have negated inputs)

    GigMode_size
};

extern cchar* GigMode_name[GigMode_size];
template<> fts_macro void write_(Out& out, const GigMode& v) { out += GigMode_name[v]; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gig data:


struct Gig_data {
    SlimAlloc<uint>     mem;

    uint                frozen;         // -- if non-zero, netlist is read-only (1=constant, 2=canonical)
    GigMode             mode_;          // }- restriction on which gate types are allowed (mode
    uint64              mode_mask;      // }  mask will always exlude 'gate_NULL' and 'gate_Const')
    uint64              strash_mask;    // -- Subset of 'mode_mask' that is allowed in strashed mode (excluding strashed gate types)

  #if defined(ZZ_GIG_PAGED)
    Vec<Gate*>          pages;
  #else
    Vec<Gate>           gates;
  #endif
    Vec<IdRepos>        numbers;
    Vec<Vec<gate_id> >  type_list;      // -- for selected type (attribute 'enum')
    Vec<uint>           type_count;
    uint                size_;

    bool                use_freelist;
    Vec<gate_id>        freelist;

    GigObj**            objs;
    Vec<GigLis*>        listeners[GigMsgIdx_size];

    // Side-tables: (extra attributes for selected types using 'number' as index)
    Vec<uint64>         lut6_ftb;
};


macro Gate& getGate(const Gig_data& N, gate_id id) {
    assert_debug(id < N.size_);
  #if defined(ZZ_GIG_PAGED)
    return N.pages[id >> ZZ_GIG_PAGE_SIZE_LOG2][id & (ZZ_GIG_PAGE_SIZE - 1)];
  #else
    return const_cast<Gate&>(N.gates[id]);
  #endif
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Wire:


// NOTE! Support for 'const' was dropped to simplify implementation. The 'const' declarations
// are left as documentation, but in the not enforced on 'Gig's and 'Wire's in any meaningful way.


struct Gig;

class Wire : public GLit {
    friend class Gig;
    friend void write_Wire(Out& out, const Wire& v);

    Gig* N;

    Gate& gate() const { assert_debug(isLegal()); return getGate(*((Gig_data*)N), id); }

public:
    // Constructors:
    Wire() : GLit(), N(NULL) {}

    Wire(const Gig* N_, GLit p) : GLit(p), N(const_cast<Gig*>(N_)) {}   // -- INTERNAL! Don't used directly!
    void  set_unchecked(uint pin, GLit v);                              // -- INTERNAL! Don't used directly!
    void  nl_set(Gig& N_) { N = &N_; }                                  // -- INTERNAL! Don't used directly!

    // Selectors:
    const GLit&  lit ()     const { return static_cast<const GLit&>(*this); }
    GLit&        lit ()           { return static_cast<GLit&>(*this); }
    Gig*         gig ()     const { assert_debug(isLegal()); return N; }
    uint         size()     const { return gate().size; }
    GateType     type()     const { return (GateType)gate().type; }
    GateAttrType attrType() const { return gatetype_attr[type()]; }

    // Predicates:
    bool  isLegal  () const { return id >= gid_FirstLegal; }  // -- Legal wires are any wire except 'Wire_NULL' and 'Wire_ERROR'. These have no netlists and point to no gate.
    bool  isDynamic() const { return gatetype_size[type()] == DYNAMIC_GATE_SIZE; }
    bool  isRemoved() const { return type() == gate_NULL; }

    // Fanin access:
    Wire  operator[](uint pin) const;   // -- access fanins
    void  set       (uint pin, GLit v); // -- set fanins
    void  disconnect(uint pin) { set(pin, GLit()); }

    Wire  init(GLit v0) { set(0, v0); return *this; }
    Wire  init(GLit v0, GLit v1) { set(0, v0); set(1, v1); return *this; }
    Wire  init(GLit v0, GLit v1, GLit v2) { set(0, v0); set(1, v1); set(2, v2); return *this; }
        // -- Useful with 'Gig::add()' to immediately set some inputs but still return the new wire.
        // Can also be used to set all inputs of a (small) gate in one go.

    Array<GLit> fanins() const;         // -- low-level, prefer 'For_Inputs' macro

    // Negation:
    Wire operator~()       const { return Wire(N, ~lit()); }
    Wire operator+()       const { return Wire(N, +lit()); }
    Wire operator^(bool s) const { return Wire(N, lit() ^ s); }

    // Attribute:
    uint  num() const { assert_debug(isNumberedAttr(attrType())); return gate().inl[2]; }
    uint  arg() const { assert_debug(attrType() == attr_Arg);     return gate().inl[2]; }
    lbool lb () const { assert_debug(attrType() == attr_LB );     return lbool_new(gate().inl[2]); }

    void  arg_set(uint  v) { assert_debug(((Gig_data*)N)->frozen == 0); assert_debug(attrType() == attr_Arg); gate().inl[2] = v; }
    void  lb_set (lbool v) { assert_debug(((Gig_data*)N)->frozen == 0); assert_debug(attrType() == attr_LB ); assert_debug(id >= gid_FirstUser); gate().inl[2] = v.value; }
};


inline Wire Wire::operator[](uint pin) const
{
    assert_debug(isLegal());
    const Gate& g = gate();
    assert_debug(pin < g.size);
    const uint* fanin = g.is_ext ? g.ext : g.inl;
    GLit        child(packed_, fanin[pin]);
    return Wire(N, child);
}


inline Array<GLit> Wire::fanins() const
{
    assert_debug(isLegal());
    Gate& g = gate();
    uint* fanin = g.is_ext ? g.ext : g.inl;
    return Array_new(reinterpret_cast<GLit*>(fanin), g.size);
}


void tell_update(Vec<GigLis*>& lis, uint pin, Wire w, Wire v);  // (forward declaration)


inline void Wire::set_unchecked(uint pin, GLit v)   // -- ignores invariants and listeners
{
    Gate& g = gate();
    uint* fanin = g.is_ext ? g.ext : g.inl;
    fanin[pin] = v.data();
}


inline void Wire::set(uint pin, GLit v)
{
    assert_debug(isLegal());
    assert_debug(pin < size());
    assert_debug((1ull << type()) & ((Gig_data*)N)->strash_mask); // -- if this assert fails, you are trying to change inputs of a strashed gate.
    assert_debug(v == GLit_NULL || !Wire(N, v).isRemoved());
    assert_debug(((Gig_data*)N)->frozen == 0);

    Vec<GigLis*>& lis = ((Gig_data*)N)->listeners[msgidx_Update];
    if (lis.size() > 0)
        tell_update(lis, pin, *this, Wire(N, v));

    return set_unchecked(pin, v);
}


#if defined(ZZ_CONSTANTS_AS_MACROS)
    #define Wire_NULL  Wire(NULL, GLit_NULL)
    #define Wire_ERROR Wire(NULL, GLit_ERROR)
#else
    static const Wire Wire_NULL  = Wire(NULL, GLit_NULL);
    static const Wire Wire_ERROR = Wire(NULL, GLit_ERROR);
#endif


template<> fts_macro uint64 hash_<Wire>(const Wire& w) { return hash_(w.lit()); }

macro bool operator==(Wire w, GateType t) { return w.type() == t; }
macro bool operator==(GateType t, Wire w) { return w.type() == t; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// GigRemap -- used after garbage collecting:


struct GigRemap {
    Vec<GLit> new_lit;
        // -- 'new_lit[old_id]' gives new gate literal after compaction, or 'GLit_NULL' if gate was removed.

    gate_id operator()(gate_id old) const { return new_lit[old].id; }   // -- loses sign
    GLit    operator()(GLit    old) const { return new_lit[old.id] ^ old.sign; }
    Wire    operator()(Wire    old) const { return Wire(old.gig(), new_lit[old.id] ^ old.sign); }

    template<class V>
    void applyTo(V& v) {
        for (uint i = 0; i < v.size(); i++)
            v[i] = operator()(v[i]);
    }
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// GigObj:


struct GigObj : NonCopyable {
    Gig* N; // -- all gig objects have a pointer to the netlist they belong to. Note that 'N' may
            // change value if object is moved to a new netlist; don't make a copy of this value.
    GigObj(Gig& N_) : N(&N_) {}

    virtual ~GigObj() {}
    virtual void init() { assert(false); }                     // -- called after construction when an object is added to a netlist
    virtual void load(In& in) { assert(false); };              // -- called after construction when a netlist is loaded
    virtual void save(Out& out) const { assert(false); };      // -- called when saving a netlist
    virtual void copyTo(GigObj& dst) const { assert(false); }; // -- 'dst' is constructed but not initialized (same state as for 'load()')
    virtual void compact(const GigRemap& remap) {}             // -- If you have no 'Wire's in your attribute, this should be left undefined.

    // Note that 'compact' may delete a gate (old gate is mapped to 'GLit_NULL') or map
    // equivalent gates to the same gate (old gates 'u' and 'v' are both mapped to 'w_new').
};


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// GigLis -- listener base class:


struct GigLis {
    virtual void updating  (Wire w, uint pin, Wire w_old, Wire w_new) {}
    virtual void adding    (Wire w) {}
    virtual void removing  (Wire w, bool recreated) {}
    virtual void compacting(const GigRemap& remap) {}
        // -- if 'recreated' is set in 'removing(), then the gate will be recreated in place (same
        // gate ID) immediately, so it may keep certain attributes (such as name); most often this
        // argument should be ignored though.

    virtual void substituting(Wire w_old, Wire w_new) {}
        // -- this is a user generated message (the Gig won't send it by itself (although a
        // Gig object may, such as 'Strash')); it allows for one component of the program to
        // communicate a fanout transfer to another. NOTE! 'w_old' is always unsigned.

    virtual ~GigLis() {}

    // Note that a listener may be transferred to another netlist by 'Gig::moveTo()'.
};


inline void tell_update(Vec<GigLis*>& lis, uint pin, Wire w, Wire v)    // -- used by 'Wire::set()'.
{
    Wire v_old = w[pin];
    for (uint i = 0; i < lis.size(); i++)
        lis[i]->updating(w, pin, v_old, v);
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Debug:


template<> fts_macro void write_(Out& out, const Gate& v)
{
    FWrite(out) "{type=%_; is_ext=%_; size=%_; inl=%_; attr=%_}",
        (GateType)v.type, v.is_ext, v.size,
        Array_new(v.is_ext ? v.ext : v.inl, v.size),
        v.inl[2];
}


inline void write_Wire(Out& out, const Wire& v)
{
    if (!v.isLegal()){
        if (v.id == gid_NULL)
            FWrite(out) "%CWire_NULL", v.sign?'~':0;
        else{ assert(v.id == gid_ERROR);
            FWrite(out) "%CWire_ERROR", v.sign?'~':0; }
    }else{
        FWrite(out) "%Cw%_:%_", v.sign?'~':0, v.id, v.type();
        if (isNumbered(v.type()))
            FWrite(out) "<%_>", v.num();
    }
}


inline void write_Wire(Out& out, const Wire& v, Str flags)
{
    if (flags.size() == 1 && flags[0] == 'f'){  // f == fanins included
        write_Wire(out, v);
        out += ' ', v.fanins();
    }else
        assert(false);  // -- more flags later
}


template<> fts_macro void write_(Out& out, const Wire& v) {
    write_Wire(out, v); }

template<> fts_macro void write_(Out& out, const Wire& v, Str flags) {
    write_Wire(out, v, flags); }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Gig:


struct Gig : Gig_data, NonCopyable {
    gate_id addInternal(GateType type, uint sz, uint attr, bool strash_normalized = false);
    void    loadGate(GateType type, uint sz);
    void    flushRle(Out& out, uchar type, uint count, uint end);

  //________________________________________
  //  Constructor:

    Gig();
   ~Gig();
    void  clear(bool reinit = true);   // -- if 'reinit' is FALSE, netlist may not be used after this call
    bool  isEmpty() const;

    void  moveTo(Gig& dst);
    void  copyTo(Gig& dst) const;
        // -- NOTE! You cannot copy a netlist with active listeners, unless those are also 'GigObj's.

  //________________________________________
  //  Mode control:

    void    freeze();
    void    thaw();
    bool    isFrozen() const;
    bool    isCanonical() const; // -- frozen, compacted and topologically sorted (use 'compact()' to get to this state)
    void    setFrozen(bool state);
        // -- NOTE! 'freeze/thaw' affects only the gates of the netlist, not Gig objects or listeners.

    GigMode mode() const { return mode_; }
    void    setMode(GigMode mode);
    void    assertMode() const;                 // -- validate the current mode, abort program if fails

  //________________________________________
  //  Special gates: (always present)

    Wire  Unbound () const { return Wire(this, GLit_Unbound ); }
    Wire  Conflict() const { return Wire(this, GLit_Conflict); }
    Wire  False   () const { return Wire(this, GLit_False   ); }
    Wire  True    () const { return Wire(this, GLit_True    ); }
    Wire  Reset   () const { return Wire(this, GLit_Reset   ); }

  //________________________________________
  //  Adding gates:

    Wire  add   (GateType type);                 // -- add fixed sized gate
    Wire  add   (GateType type, uint attr);
    Wire  addDyn(GateType type, uint sz);        // -- add dynamically sized gate
    Wire  addDyn(GateType type, uint sz, uint attr);
        // NOTE! In strashed mode, gates controlled by strashing (e.g. AND-gates) should be
        // constructed by functions in 'Strash.hh' ('aig_And()', 'xig_Mux()' etc.).

    void  remove(gate_id id, bool recreate = false);    // -- 'recreate' is for internal use, don't set 

  //________________________________________
  //  Gate access:

    Wire  operator[](gate_id id) const { return Wire(this, GLit(id)); }
    Wire  operator[](GLit    p ) const { return Wire(this, p); }

    Wire  enumGate(GateType type, uint num) const { return Wire(this, GLit(type_list[type][num])); }
    uint  enumSize(GateType type)           const { return type_list[type].size(); }

  //________________________________________
  //  Gate count:

    uint  size     () const { return size_; }
    uint  nRemoved () const { return type_count[gate_NULL] - ZZ_GLIT_NULL_GATES; }
    uint  count    () const { return size() - type_count[gate_NULL] - (gid_FirstUser - gid_FirstLegal); }
    uint  typeCount(GateType type) const { return type_count[type]; }
        // -- 'size' is the underlying size of the netlist (= number of slots for gates, deleted or not)
        // which can be used with 'operator[]'. 'nRemoved()' and 'count()' only includes user gates.

  //________________________________________
  //  Listeners:

    void  listen  (GigLis& lis, uint msg_mask); // -- register a listener for messages 'msg_mask'.
    void  unlisten(GigLis& lis, uint msg_mask); // -- remove a previously registered listener (mask must not include unregistered messages).

    void  tell_subst(GLit w_old, GLit w_new);

  //________________________________________
  //  Side-tables: (always present)

    void  clearFtbs() { lut6_ftb.clear(true); }
        //  -- the actual tables are accessed through the functions at the bottom of this file

  //________________________________________
  //  Objects: (selectively present)

    bool    hasObj(GigObjType obj_idx) const { return objs[obj_idx] != NULL; }
    void    addObj(GigObjType obj_idx)       { assert_debug(!hasObj(obj_idx)); gigobj_factory_funcs[obj_idx](*this, objs[obj_idx], true); }
    GigObj& getObj(GigObjType obj_idx)       { assert_debug(hasObj(obj_idx)); return *objs[obj_idx]; }
    void    removeObj(GigObjType obj_idx)    { assert_debug(hasObj(obj_idx)); delete objs[obj_idx]; objs[obj_idx] = NULL; }
        // -- 'GigExtra.hh' contains functions for accessing netlist objects.

  //________________________________________
  //  Garbage collecting:

    bool  isRecycling() const          { return use_freelist; }
    void  setRecycling(bool on = true) { use_freelist = on; }

    void  clearNumbering(GateType type);
        // -- If all gates of a type have been removed, this can be used to reset the automatic
        // numbering scheme (which by default provides numbers in reverse order of freeing due to
        // the freelist implementation).

    void  compact(bool remove_unreach = true, bool set_canonical = true);
    void  compact(GigRemap& remap, bool remove_unreach = true, bool set_canonical = true);
        // -- Will topologically order the gates and remove any gaps in the gate tables
        // created by gate removal. By default, unreachable gates (from COs) are first removed.
        // Once done, 'compact()' will leave the netlist in a frozen, canonical mode.

  //________________________________________
  //  Disk:

    void load(In& in);      // -- may throw 'Excp_Msg'
    void save(Out& out);

    void load(String filename) {
        InFile in(filename);
        if (in.null()) Throw(Excp_Msg) "Could not open file for reading: ", filename;
        load(in); }

    void save(String filename) {
        OutFile out(filename);
        if (out.null()) Throw(Excp_Msg) "Could not open file for writing: ", filename;
        save(out); }
};


macro Wire operator+(gate_id id, const Gig& N) { return N[id]; }
macro Wire operator+(GLit p    , const Gig& N) { return N[p];  }
    // -- alternative syntax for combinining netlist + gate literal (or ID) into a wire.


//=================================================================================================
// -- Larger inlines:


inline Gig::Gig()
{
    frozen       = 0;
    mode_        = gig_FreeForm;
    mode_mask    = 0;
    strash_mask  = 0;
    size_        = 0;
    use_freelist = true;
    objs         = NULL;

    clear(true);
}


inline Gig::~Gig() {
    clear(false); }


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


inline bool Gig::isFrozen() const {
    return frozen >= 1; }

inline bool Gig::isCanonical() const {
    return frozen >= 2; }

inline void Gig::freeze() {
    if (!frozen) frozen = 1; }

inline void Gig::thaw() {
    frozen = 0; }

inline void Gig::setFrozen(bool state) {
    if (state) freeze();
    else       thaw(); }


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


inline Wire Gig::add(GateType type)
{
    uint sz   = gatetype_size[type]; assert_debug(sz != DYNAMIC_GATE_SIZE);
    uint attr = isNumbered(type) ? numbers[type].get() : 0;
    return Wire(this, GLit(addInternal(type, sz, attr)));
}


inline Wire Gig::add(GateType type, uint attr)
{
    uint sz = gatetype_size[type]; assert_debug(sz != DYNAMIC_GATE_SIZE);
    if (isNumbered(type))
        numbers[type].pick(attr);
    return Wire(this, GLit(addInternal(type, sz, attr)));
}


inline Wire Gig::addDyn(GateType type, uint sz)
{
    assert_debug(gatetype_size[type] == DYNAMIC_GATE_SIZE);
    uint attr = isNumbered(type) ? numbers[type].get() : 0;
    return Wire(this, GLit(addInternal(type, sz, attr)));
}


inline Wire Gig::addDyn(GateType type, uint sz, uint attr)
{
    assert_debug(gatetype_size[type] == DYNAMIC_GATE_SIZE);
    if (isNumbered(type))
        numbers[type].pick(attr);
    return Wire(this, GLit(addInternal(type, sz, attr)));
}


//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


inline void Gig::tell_subst(GLit w_old, GLit w_new)
{
    if (w_old.sign){
        w_old = ~w_old;
        w_new = ~w_new; }

    Vec<GigLis*>& lis = listeners[msgidx_Subst];
    for (uint i = 0; i < lis.size(); i++)
        lis[i]->substituting(Wire(this, w_old), Wire(this, w_new));
}


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Functions:


macro void remove(Wire w) {
    w.gig()->remove(w.id); }


#define Wrap(expr)                              \
    Gig& N = *w.gig();                          \
    bool tmp = N.use_freelist;                  \
    N.use_freelist = true;                      \
    N.remove(w.id, true);                       \
    Wire ret = expr;                            \
    N.use_freelist = tmp;                       \
    return ret;

macro Wire  change   (Wire w, GateType type)                     { Wrap(N.add(type)) }
macro Wire  change   (Wire w, GateType type, uint attr)          { Wrap(N.add(type, attr)); }
macro Wire  changeDyn(Wire w, GateType type, uint sz)            { Wrap(N.addDyn(type, sz)); }
macro Wire  changeDyn(Wire w, GateType type, uint sz, uint attr) { Wrap(N.addDyn(type, sz, attr)); }

#undef Wrap


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
// Side-tables access:


macro uint64& ftb(Wire w) {     // -- the returned reference is invalidated if a gate is added
    assert_debug(w.type() == gate_Lut6);
    return w.gig()->lut6_ftb[w.num()]; }


//mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
}
#endif
