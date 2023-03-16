#ifndef ADS_SET_H
#define ADS_SET_H

#include <functional>
#include <algorithm>
#include <iostream>
#include <stdexcept>

template <typename Key, std::size_t N = 23>
class ADS_set {
public:
  class Iterator;
  using value_type = Key;
  using key_type = Key;
  using reference = key_type &;
  using const_reference = const key_type &;
  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using iterator = Iterator;
  using const_iterator = Iterator;
  using key_compare = std::less<key_type>;   // B+-Tree
  using key_equal = std::equal_to<key_type>; // Hashing
  using hasher = std::hash<key_type>;        // Hashing
private:
    enum class Mode{free, used, end}; //tells if the index is used or free 

    struct element{
        key_type key; //value to be saved
        Mode mode{Mode::free};
        int next{-1}; //contains index of the next element in chain if a collision happens
        int prev{-1}; //needed for erase to connect element before deleted element to the rest of the chain.
    };

    element *table{nullptr};
    float max_lf{0.7}; //entries in table should not exceed this factor
    float cellar{0.1628}; //cellar cannot be reached by hash function only if collision occurs
    size_type curr_size{0}, table_size{0}, table_last{0}, full_table{0};

    size_type h(const key_type& key) const { return hasher{}(key) % table_size; } //hash function
    size_type g(const key_type&) { return (--table_last) % full_table; } //if collision occurs

    void reserve(size_type n);
    void rehash(size_type n); 

    element *insert_(const key_type &key);
    element *find_(const key_type &key) const;

public:
  ADS_set()
  {
    rehash(N);
  }
  ADS_set(std::initializer_list<key_type> ilist): ADS_set{}
  {
    insert(ilist);
  }
  template<typename InputIt> ADS_set(InputIt first, InputIt last): ADS_set{}
  { 
    insert(first, last); 
  }
  ADS_set(const ADS_set &other): ADS_set{} {
    reserve(other.curr_size);
  	for(const auto &k: other) { //range-based for loop setzt Iterator vorraus (wir haben einen definiert)
  		insert_(k);
  	}
}
  ~ADS_set() { delete[] table; }

  ADS_set &operator=(const ADS_set &other) {
  	if(this == &other) return *this;
  	ADS_set tmp{other};
  	swap(tmp);
  	return *this;
  }
  ADS_set &operator=(std::initializer_list<key_type> ilist){
  	ADS_set tmp{ilist};
  	swap(tmp);
  	return *this;
  }

  size_type size() const
  {
      return curr_size;
  }
  bool empty() const
  {
      return curr_size == 0;
  }

  size_type count(const key_type &key) const
  {
      return !!find_(key);
  }
  iterator find(const key_type &key) const {
  	if(auto p {find_(key)}) return iterator{p};
  	return end();
  }

  void clear() {
  	ADS_set tmp;
  	swap(tmp);
  }
  void swap(ADS_set &other) {
  	using std::swap;
  	swap(table, other.table);
  	swap(curr_size, other.curr_size);
  	swap(table_size, other.table_size);
  	swap(max_lf, other.max_lf);
  	swap(full_table, other.full_table);
  	swap(table_last, other.table_last);
  	swap(cellar, other.cellar);
  }

  void insert(std::initializer_list<key_type> ilist)
  {
      insert(std::begin(ilist), std::end(ilist));
  }
  std::pair<iterator,bool> insert(const key_type &key) {
  	if(auto p {find_(key)}) return {iterator{p}, false};
  	reserve(curr_size+1);
  	return {iterator{insert_(key)}, true};
  }

  template<typename InputIt> void insert(InputIt first, InputIt last);

  size_type erase(const key_type &key) {

  	//first check if the key to be deleted is in the database
  	if(auto p{find_(key)}) {
  		int nextone{p->next};
  		int again{nextone};
  		int previous{p->prev};

		//std::cout << "The next in Chain is: " << nextone << std::endl;
		  		p->mode = Mode::free;
		  		p->next = -1;
          p->prev = -1;
		  		--curr_size;
		  		//std::cout << "Previous one: " << previous << std::endl;
		  		if(previous != -1) table[previous].next = -1;

  		while(nextone != -1){
	  		again = table[again].next;
	  			
		  		//when there is no collision do this with the next object in chain
		  		auto index{h(table[nextone].key)};
          table[nextone].next = -1;
		  		//std::cout << "The hash index of nextone is: " << index << std::endl;
		  		if(table[index].mode == Mode::free){
            --curr_size;
		  			insert_(table[nextone].key);
		  			table[nextone].mode = Mode::free;
		  			table[nextone].next = -1;
            table[nextone].prev = -1;
		  		}else{ //if there is a collision do this
		  			int chain{table[index].next};
		  			while(chain != -1){
		  				index = chain;
		  				//std::cout << "while " << index << std::endl;
		  				chain = table[chain].next;
		  			}
		  			//std::cout << "before " << table[index].next << std::endl;
		  			table[index].next = nextone;
		  			table[nextone].prev = index;
		  			//std::cout << "after " << table[index].next << std::endl;
		  		}
		  	nextone = again;	
		  }

	  	table_last = full_table;
	  	return 1;
  	}
  	
  	return 0;
}

  const_iterator begin() const { 
	return const_iterator{table}; 
  }
  const_iterator end() const { 
	return const_iterator{table+full_table}; 
  }

  void dump(std::ostream &o = std::cerr) const;

  friend bool operator==(const ADS_set &lhs, const ADS_set &rhs) {
  	if(lhs.curr_size != rhs.curr_size) return false;
  	for(const auto &k: rhs) if(!lhs.count(k)) return false;
  	return true;
  }
  friend bool operator!=(const ADS_set &lhs, const ADS_set &rhs) { return !(lhs == rhs); }
};

/////////////////////////////////////////////
////////////// PHASE 1 //////////////////////
/////////////////////////////////////////////

template<typename Key, std::size_t N>
void ADS_set<Key, N>::reserve(size_type n)
{
  
    if(n > table_size * max_lf){
        size_type new_table_size {table_size};
        
        do{
            new_table_size = new_table_size * 2 + 1;
        }while(n > new_table_size * max_lf);

        rehash(new_table_size);
    }
}


template<typename Key, std::size_t N>
void ADS_set<Key, N>::rehash(size_type n)
{
    size_type new_table_size(std::max(N, std::max(n, static_cast<size_type>(curr_size/max_lf))));
    size_type new_full_table(new_table_size + static_cast<size_type>(new_table_size*cellar));

    element *new_table{new element[new_full_table+1]};
    new_table[new_full_table].mode = Mode::end;

    size_type old_full_table{full_table};
    element *old_table{table};
    curr_size = 0;
    table = new_table;
    table_size = new_table_size;
    full_table = new_full_table;
    table_last = new_full_table;

    for(size_type idx{0}; idx < old_full_table; ++idx){
        if(old_table[idx].mode == Mode::used) insert_(old_table[idx].key);
    }

    delete[] old_table;
}

template<typename Key, std::size_t N>
typename ADS_set<Key, N>::element *ADS_set<Key, N>::insert_(const key_type &key)
{
  size_type idx {h(key)};
  if(table[idx].mode == Mode::free){

    table[idx].key = key;
    table[idx].mode = Mode::used;
    table[idx].next = -1;
    table[idx].prev = -1;
    ++curr_size;

    return table+idx;

  }

  int last_in_chain{-1};
  int next_ele{table[idx].next};

  if(table[idx].mode == Mode::used){

    if(next_ele == -1)
      last_in_chain = idx;

    else{

      do{
        last_in_chain = next_ele;
        next_ele = table[next_ele].next;
      }while(next_ele != -1);

    }
    
    while(table[idx].mode != Mode::free){
      idx = g(key);
    }

    table[idx].key = key;
    table[idx].mode = Mode::used;
    table[last_in_chain].next = idx;
    table[idx].next = -1;
    table[idx].prev = last_in_chain;
    ++curr_size;

  }

  return table+idx;
}


template<typename Key, std::size_t N>
typename ADS_set<Key,N>::element *ADS_set<Key,N>::find_(const key_type &key) const
{
  size_type idx {h(key)};
  int next_ele{table[idx].next};
    switch(table[idx].mode){

      case Mode::free: 
        return nullptr;
      case Mode::used: 

        if(key_equal{}(table[idx].key, key)) return table+idx;

        if(next_ele == -1) return nullptr;

        do{
          if (table[next_ele].mode == Mode::free) return nullptr;
          if (key_equal{}(table[next_ele].key, key)) return table+next_ele;
          next_ele = table[next_ele].next;
        }while(next_ele != -1);
      case Mode::end:
        return nullptr;
       	//throw std::runtime_error("find_ reached END");
    }

  return nullptr;

}

template <typename Key, std::size_t N>
template<typename InputIt> void ADS_set<Key, N>::insert(InputIt first, InputIt last)
{
  for(auto it{first}; it != last; ++it){
    if(!count(*it)){
      reserve(curr_size+1);
      insert_(*it);
    }
  }
}

template <typename Key, std::size_t N>
void ADS_set<Key, N>::dump(std::ostream &o) const
{
  o << "curr_size = " << curr_size << " table_size = " << table_size << " full_table = " << full_table << " table_last = " << table_last <<  std::endl;
  for(size_type idx{0}; idx < full_table+1; ++idx){
    o << idx << ": ";
    switch(table[idx].mode){
      case Mode::free: 
        o << "--free"; 
        break;
      case Mode::used: 
        o << table[idx].key << ", next: " << table[idx].next << ", prev: " << table[idx].prev;
        break;
      case Mode::end: 
        o << "--END";
        break;
    }
    o << std::endl;
  }
}


/////////////////////////////////////////////
////////////// PHASE 2 //////////////////////
/////////////////////////////////////////////


/*template <typename Key, std::size_t N>
typename ADS_set<Key, N>::ADS_set(const ADS_set &other)
{
  	for(const auto &k: other): ADS_set{} { //range-based for loop setzt Iterator vorraus (wir haben einen definiert)
  		reserve(other.curr_size);
  		insert_(k);
  	}
}

template <typename Key, std::size_t N>
typename ADS_set<Key, N>::ADS_set &ADS_set<Key, N>::operator=(const ADS_set &other) 
{
  	if(this == &other) return *this;
  	ADS_set tmp{other};
  	swap(tmp);
  	return *this;
}

template <typename Key, std::size_t N>
typename ADS_set<Key, N>::ADS_set &ADS_set<Key, N>::operator=(std::initializer_list<key_type> ilist) 
{
  	ADS_set tmp{ilist};
  	swap(tmp);
  	return *this;
}


template <typename Key, std::size_t N>
typename ADS_set<Key, N>::iterator ADS_set<Key, N>::find(const key_type &key) const 
{
  	if(auto p {find_(key)}) return iterator{p};
  	return end();
}

template <typename Key, std::size_t N>
void ADS_set<Key, N>::clear() 
{
  	ADS_set tmp;
  	swap(tmp);
}

template <typename Key, std::size_t N>
void ADS_set<Key, N>::swap(ADS_set &other) 
{
  	using std::swap;
  	swap(table, other.table);
  	swap(curr_size, other.curr_size);
  	swap(table_size, other.table_size);
  	swap(max_lf, other.max_lf);
  	swap(full_table, other.full_table);
  	swap(table_last, other.table_last);
  	swap(cellar, other.cellar);
}

template <typename Key, std::size_t N>
typename std::pair<ADS_set<Key, N>::iterator,bool> ADS_set<Key, N>::insert(const key_type &key)
{
  	if(auto p {find_(key)}) return {iterator{p}; false};
  	reserve(curr_size+1);
  	return {iterator{insert_(key)}, true};
}


template <typename Key, std::size_t N>
typename ADS_set<Key, N>::const_iterator ADS_set<Key, N>::begin() const 
{ 
	return const_iterator{table}; 
}

template <typename Key, std::size_t N>
typename ADS_set<Key, N>::const_iterator ADS_set<Key, N>::end() const 
{ 
	return const_iterator{table+table_size}; 
}*/


/*
Löschvorgang:
Wir suchen den zu löschenden Wert und merken uns bei der Suche immer den jeweiligen Vorgänger. 
Für den ersten Wert in der Kette ist der Vorgänger leer).  

Wurde der Wert nicht gefunden, return.  

Der Wert wurde an einer Position pos gefunden und der Vorgänger pre ist bekannt. 
Wenn pre nicht leer ist, dann kappe den Link von pre (pre ist dann das Ende der Kette,
in der es vorkommt). 

Merken des Nachfolgers von pos in einer Variablen kette 
(das ist quasi die Kette aller Elemente, die an dem zu löschenden hängen) pos löschen 
(status auf leer setzen, kein Nachfolger). 

Damit haben wir eine Lücke in der Tabelle an der Position pos erzeugt. 
Wir durchlaufen nun alle Elemente von kette.     
Für jedes Element berechnen wir den Hashwert h    
Es gibt zwei Fälle:      

h trifft die Lücke:         
Wir kopieren das Element von seiner aktuellen Position in die Lücke 
(diese hat keinen Nachfolger)          
Dadurch entsteht eine neue Lücke an dieser aktuellen Position. Diese wird wieder geleert 
(Inhalt löschen, kein Nachfolger)       

h trifft eines der zuvor schon abgearbeiteten Elemente aus unserer Kette         
Das Element bleibt an seiner aktuellen Posiiton und wird dann einfach an das Ende der 
Kette, die von diesem getroffenen Element ausgeht, gehängt. Die Kette hört bei dem 
angehängten (aktuellen) Element auf. Am Ende bleibt eine Lücke, die schon ordnungsgemäß 
geleert worden sein sollte. Das ist quasi die neue leere Stelle, die durch das Löschen 
erzeugt wurde.
*/


/*template <typename Key, std::size_t N>
typename ADS_set<Key, N>::size_type ADS_set<Key, N>::erase(const key_type &key) 
{
	std::vector<element> kette;
	//int last{0};

  	if(auto p{find_(key)}) {

  		auto nextone{p->next};
  		if(nextone->mode == Mode::used){
  			do{
  				kette.push_back(nextone);
  				nextone->mode = Mode::free;
  				nextone = nextone->next;
  			}while(nextone->mode == Mode::used);
  		} 

  		p->mode = Mode::free;
  		--curr_size;

  		if(kette.size() != 0){
  			for(const auto &i: kette){
  				insert_(i);
  			}
  		}

  		return 1;
  	}
  	return 0;
}*/




/////////////////////////////////////////////
////////////// ITERATOR /////////////////////
/////////////////////////////////////////////


template <typename Key, size_t N>
class ADS_set<Key,N>::Iterator {
public:
  using value_type = Key;
  using difference_type = std::ptrdiff_t;
  using reference = const value_type &;
  using pointer = const value_type *;
  using iterator_category = std::forward_iterator_tag;

private:
	element *pos;
	void skip() {
	while(pos->mode != Mode::used && pos->mode != Mode::end) ++pos;
    }

public:
  explicit Iterator(element *pos = nullptr):  pos{pos} { if(pos) skip(); } //Iterator muss erzeugt werden koennen auch wenn man ihn nicht benutzt.
  reference operator*() const {
	return pos->key;
  }
  pointer operator->() const {
	return &pos->key;
  }
  Iterator &operator++() {
	if(pos->mode != Mode::end) ++pos;
	skip();
	return *this;
  }
  Iterator operator++(int) {
	auto rc{*this};
	++*this;
	return rc;
  }
  friend bool operator==(const Iterator &lhs, const Iterator &rhs) { return lhs.pos == rhs.pos; }
  friend bool operator!=(const Iterator &lhs, const Iterator &rhs) { return !(lhs == rhs);  }
};

template <typename Key, size_t N> void swap(ADS_set<Key,N> &lhs, ADS_set<Key,N> &rhs) { lhs.swap(rhs); }


/*template<typename Key, std::size_t N>
void ADS_set<Key,N>::Iterator::skip() 
{
	while(pos->mode != Mode::used && pos->mode != Mode::end) ++pos;
}

template<typename Key, std::size_t N>
typename ADS_set<Key,N>::Iterator::reference Iterator::operator*() const 
{
	return pos->key;
}

template<typename Key, std::size_t N>
typename ADS_set<Key,N>::Iterator::pointer ADS_set<Key, N>:: operator->() const 
{
	return &pos->key;
}

template<typename Key, std::size_t N>
typename ADS_set<Key,N>::Iterator::Iterator& ADS_set<Key, N>::operator++() 
{
	if(pos->mode != Mode::end) ++pos;
	skip();
	return *this;
}

template<typename Key, std::size_t N>
typename ADS_set<Key,N>::Iterator::Iterator ADS_set<Key, N>::operator++(int) 
{
	auto rc{*this};
	++*this;
	return rc;
}*/



#endif // ADS_SET_H