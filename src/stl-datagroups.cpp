/**
 * \example stl-datagroups.cpp
 * STL Test for Algorithms based on fixed sized workgroups
 */

#include <algorithm>
#include <execution>
#include <mutex>
#include <iostream>
#include <array>
#include <vector>
#include <goopax>

//iterators
template<typename array_t>
struct group_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = typename array_t::size_type;
    using difference_type = typename array_t::size_type;
    using pointer = group_iterator*;
    using reference = group_iterator&;
    reference operator++() { curr_group_no++; return *this; }
    reference operator++(int) { group_iterator retval = *this; ++(*this); return retval; }
    bool operator==(group_iterator other) const { return curr_group_no == other.curr_group_no; }
    bool operator!=(group_iterator other) const { return curr_group_no != other.curr_group_no; }
    value_type operator*() {
        return curr_group_no;
    }
    //private:
    typename array_t::iterator& container_start;
    typename array_t::iterator& container_end;
    typename array_t::size_type& block_size;
    value_type curr_group_no = 0;
};

template<typename array_t>
struct local_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = typename array_t::size_type;
    using difference_type = typename array_t::size_type;
    using pointer = local_iterator*;
    using reference = local_iterator&;
    reference operator++() { curr_item_no++; return *this; }
    reference operator++(int) { local_iterator retval = *this; ++(*this); return retval; }
    bool operator==(local_iterator other) const { return group_no == other.group_no && curr_item_no == other.curr_item_no; }
    bool operator!=(local_iterator other) const { return !(group_no == other.group_no && curr_item_no == other.curr_item_no);}
    value_type operator*() {
        return group_no*block_size+curr_item_no;
    }
    //private:
    typename array_t::iterator& container_start;
    typename array_t::iterator& container_end;
    typename array_t::size_type& block_size;
    value_type group_no = 0;
    value_type curr_item_no = 0;
};

template<typename array_t>
class datagroups
{
public:
    datagroups(typename array_t::iterator container_start,typename array_t::iterator container_end,typename array_t::size_type block_size) : container_start(container_start),container_end(container_end),block_size(block_size),_global{container_start, container_end, block_size} {}
    group_iterator<array_t> begin_group() { group_iterator<array_t> retval = this->_global; retval.curr_group_no = 0; return retval; }
    group_iterator<array_t> end_group() {
        group_iterator<array_t> retval = this->_global; retval.curr_group_no = data_size()/block_size;
        return retval;
    }
    local_iterator<array_t> begin_local(typename array_t::size_type group_index) { return local_iterator<array_t>{container_start,container_end,block_size,group_index}; }
    local_iterator<array_t> end_local(typename array_t::size_type group_index) { return local_iterator<array_t>{container_start,container_end,block_size,group_index,block_size};}

private:
    typename array_t::size_type data_size() { return std::distance(container_start,container_end); }
    typename array_t::iterator container_start;
    typename array_t::iterator container_end;
    typename array_t::size_type block_size;
    group_iterator<array_t> _global;
};
template<> class datagroups<goopax::resource<int>> {
public:
    datagroups(typename goopax::resource<int>::iterator container_start,typename goopax::resource<int>::iterator container_end,typename goopax::resource<int>::size_type block_size) : container_start(container_start),container_end(container_end),block_size(block_size){}
    typename goopax::resource<int>::size_type begin_group() { return 0;}
    typename goopax::resource<int>::size_type end_group() { return data_size(); }
    typename goopax::resource<int>::size_type begin_local(typename goopax::resource<int>::size_type group_index) { return group_index*block_size; }
    typename goopax::resource<int>::size_type end_local(typename goopax::resource<int>::size_type group_index) { return group_index;}

private:
    typename goopax::resource<int>::size_type data_size() { return goopax::resource<int>::size_type(container_end - container_start); }
    typename goopax::resource<int>::iterator container_start;
    typename goopax::resource<int>::iterator container_end;
    typename goopax::resource<int>::size_type block_size;
};

//functors
template<typename array_t, typename local_t> class Group_Scope {
public:
    Group_Scope(array_t& container,local_t& local_memory,std::mutex& m) : DATA(container), local_memory(local_memory), local_barrier(m) { std::cout << "Initialised Functor." << std::endl; }
    ~Group_Scope(){ std::cout << "Destroyed Functor with local_memory " << local_memory << "."<<std::endl; }
    void operator()(typename array_t::size_type n){
        const typename array_t::size_type num_registers = 256;
        const typename array_t::size_type num_loopindices = 2;//index + jump
        const typename array_t::size_type num_variables = 2;//sum + function argument
        std::vector<typename array_t::value_type> registers(num_registers-num_loopindices-num_variables);
        for (int i = 0; i < (num_registers-num_loopindices-num_variables-num_loopindices-num_variables) && i<DATA[n]; i++)
            registers[i] = i;
        for (int i = 0; i< (num_registers-num_loopindices-num_variables-num_loopindices-num_variables) && i<DATA[n]; i++)
            registers[num_registers-num_loopindices-num_variables-1-num_loopindices-1] = registers[num_registers-num_loopindices-num_variables-1-num_loopindices-1] + registers[i];
        local_lock();
        local_memory += registers[num_registers-num_loopindices-num_variables-1-num_loopindices-1];
    }
    local_t& local_memory;
private:
    void local_lock() { std::lock_guard<std::mutex> guard(local_barrier); }
    std::mutex& local_barrier;
    array_t& DATA;
};
template<> Group_Scope<goopax::resource<int>,goopax::local_mem<int>>::Group_Scope(goopax::resource<int>& container,goopax::local_mem<int>& local_memory,std::mutex& m) : DATA(container), local_memory(local_memory), local_barrier(m) { goopax::gpu_ostream out(std::cout); out << "Initialised Functor." << std::endl;}
template<> Group_Scope<goopax::resource<int>,goopax::local_mem<int>>::~Group_Scope() { goopax::gpu_ostream out(std::cout); out << "Destroyed Functor with local_memory " << std::endl; }// << local_memory[0] << "."<<std::endl; }
template<> void Group_Scope<goopax::resource<int>,goopax::local_mem<int>>::local_lock() { goopax::local_barrier(); }
template<> void Group_Scope<goopax::resource<int>,goopax::local_mem<int>>::operator()(typename goopax::resource<int>::size_type n){
    goopax::private_mem<int> registers(252);
    goopax::gpu_int my_n = DATA[n];
    goopax::gpu_for (0, DATA[n],[&registers](goopax::gpu_uint i){
        //goopax::gpu_if(i < (256-2-2-2-2)){
        registers[i] = i;
        //}
    });
    goopax::gpu_for (0, DATA[n],[&registers](goopax::gpu_uint i){
        //goopax::gpu_if(i < (256-2-2-2-2)){
        registers[256-2-2-1-2-1] = registers[256-2-2-1-2-1] + registers[i];
        //}
    });
    local_lock();
    local_memory[0] += registers[256-2-2-1-2-1];
}

template<typename array_t, typename global_t> class Global_Scope {
public:
    Global_Scope(array_t& container, global_t& global_memory) : DATA(container), global_memory(global_memory) {}
    template<typename local_t> global_t& operator()(local_t& s) {
        global_barrier.lock();
        global_memory += s;
        global_barrier.unlock();
        return global_memory;
    }
    array_t& DATA;
    global_t& global_memory;
private:
    void global_lock() { std::lock_guard<std::mutex> guard(global_barrier); }
    std::mutex global_barrier;
};
template<> void Global_Scope<goopax::resource<int>,goopax::resource<int>>::global_lock() { goopax::local_barrier(); }
template<>template<> goopax::resource<int>& Global_Scope<goopax::resource<int>,goopax::resource<int>>::operator()<goopax::local_mem<int>>(goopax::local_mem<int>& n){
    goopax::global_barrier();
    global_memory[0] += n[0];
    return global_memory;
}

int main()
{
    std::array<int,9> container{32,64,96,128,160,192,224,256,0};
    const std::size_t local_size = 3;
    const std::size_t global_size = std::size(container);
    auto it = datagroups<decltype(container)>{std::begin(container),std::end(container),local_size};
    typename decltype(container)::value_type global_memory=0;
    Global_Scope<decltype(container),typename decltype(container)::value_type> gather(container,global_memory);
    std::for_each(std::execution::par_unseq,it.begin_group(),it.end_group(),[&gather,&it](auto group_index){
        std::mutex local_barrier;
        typename decltype(container)::value_type local_memory=0;
        std::for_each(std::execution::par_unseq,it.begin_local(group_index),it.end_local(group_index),Group_Scope<decltype(container),typename decltype(container)::value_type>(gather.DATA,local_memory,local_barrier));
        gather(local_memory);
    } );
    std::cout << gather.global_memory << " should be 101860" << std::endl;

    auto dev = goopax::default_device(goopax::env_CPU);
    auto arg_a = goopax::buffer<int>(dev,std::size(container),container.data());
    auto arg_b = goopax::buffer<decltype(global_memory)>(dev,1,&global_memory);
    auto k = goopax::kernel(dev,[&local_size](goopax::resource<int>& container_,goopax::resource<decltype(global_memory)>& global_memory_)->void{
        auto it_ = datagroups<goopax::resource<int>>{std::begin(container_),std::end(container_),local_size};
        Global_Scope<goopax::resource<int>,goopax::resource<int>> gather_(container_,global_memory_);
        goopax::gpu_for_group(it_.begin_group(),it_.end_group(),[&gather_,&it_](goopax::gpu_uint group_index){
            std::mutex local_barrier;
            goopax::local_mem<int> local_memory=0;
            goopax::gpu_for_local(it_.begin_local(group_index),it_.end_local(group_index),Group_Scope<std::remove_reference<decltype(container_)>::type,goopax::local_mem<int>>(gather_.DATA,local_memory,local_barrier));
            gather_(local_memory);
        });
    });
    k(arg_a,arg_b);
    std::cout << arg_b.to_vector()[0] << " should be 101860" << std::endl;

}