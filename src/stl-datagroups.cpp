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
    typename goopax::resource<int>::size_type end_group() { return data_size()/block_size; }
    typename goopax::resource<int>::size_type begin_local(typename goopax::resource<int>::size_type group_index) { return 0; }
    typename goopax::resource<int>::size_type end_local(typename goopax::resource<int>::size_type group_index) { return block_size;}

private:
    typename goopax::resource<int>::size_type data_size() { return goopax::resource<int>::size_type(container_end - container_start); }
    typename goopax::resource<int>::iterator container_start;
    typename goopax::resource<int>::iterator container_end;
    typename goopax::resource<int>::size_type block_size;
};

int main()
{
    std::array<int,9> DATA{32,64,96,128,160,192,224,256,0};
    const unsigned int group_size = 3;
    const unsigned int global_size = std::size(DATA);
    auto it = datagroups<decltype(DATA)>{std::begin(DATA),std::end(DATA),group_size};
    typename decltype(DATA)::value_type global_memory_a=0;
    std::mutex local_barrier;
    auto gather = [&global_memory_a,&local_barrier](typename decltype(DATA)::value_type& value) {
        std::lock_guard<std::mutex> guard(local_barrier);
        global_memory_a += value;
    };
    std::for_each(std::execution::par_unseq,it.begin_group(),it.end_group(),[&gather,&DATA,&it,&group_size](auto group_index){
        std::mutex local_barrier;
        typename decltype(DATA)::value_type local_memory=0;
        std::for_each(std::execution::par_unseq,it.begin_local(group_index),it.end_local(group_index),
                      [&DATA,&local_memory,&local_barrier](typename decltype(DATA)::size_type global_id){
                          const typename decltype(DATA)::size_type num_registers = 255;
                          const typename decltype(DATA)::size_type num_loopindices = 2;//index + jump
                          const typename decltype(DATA)::size_type num_variables = 2;//sum + function argument
                          std::vector<typename decltype(DATA)::value_type> registers(num_registers-num_loopindices-num_variables);
                          auto bounds = DATA[global_id] < std::size(registers) ? DATA[global_id] : std::size(registers);
                          for (int i = 0; i < std::size(registers); ++i)
                              if (i<DATA[global_id])
                                  registers[i] = i;
                          typename decltype(DATA)::value_type sum = 0;
                          for (int i = 0; i< std::size(registers); ++i)
                              if (i<DATA[global_id])
                                  sum += registers[i];
                          std::lock_guard<std::mutex> guard(local_barrier);
                          local_memory += sum;
                      }
        );
        gather(local_memory);
    } );
    std::cout << global_memory_a << " should be 102607" << std::endl;

    auto dev = goopax::default_device(goopax::env_ALL);
    typename decltype(DATA)::value_type global_memory_b=0;
    auto arg_a = goopax::buffer<int>(dev,std::size(DATA),DATA.data());
    auto arg_b = goopax::buffer<decltype(global_memory_b)>(dev,1,&global_memory_b);
    auto k = goopax::kernel(dev,[&group_size](goopax::resource<int>& DATA,goopax::resource<decltype(global_memory_b)>& global_memory_b)->goopax::gather_result<int>{
        auto it_ = datagroups<goopax::resource<int>>{std::begin(DATA),std::end(DATA),group_size};
        using local_mem_t = std::vector<goopax::gpu_int>;
        //using local_mem_t = goopax::local_mem<int>;
        local_mem_t local_memory(1);
        local_memory[0] = 0;
        goopax::gpu_for_group(it_.begin_group(),it_.end_group(),[&DATA,&local_memory,&it_,&group_size](goopax::gpu_uint group_index){
            goopax::gpu_for_local(it_.begin_local(group_index),it_.end_local(group_index),
                                  [&DATA,&local_memory,&group_size,&group_index](typename goopax::resource<int>::size_type local_id){
                                      typename goopax::resource<int>::size_type global_id = group_index*group_size+local_id;
                                      goopax::private_mem<int> registers(251);
                                      for (int i=0; i<registers.size();++i){
                                          registers[i] = 0;
                                      }
                                      for (int i=0; i < registers.size(); ++i){
                                          gpu_if(i<DATA[global_id]){
                                              registers[i] = i;
                                          }
                                      }
                                      goopax::gpu_uint sum = 0;
                                      for (int i=0; i < registers.size(); ++i){
                                          gpu_if(i<DATA[global_id]){
                                              sum += registers[i];
                                          }
                                      }
                                      local_memory[0] += sum;
                                  });
        });
        return goopax::gather_add<decltype(local_memory)::value_type>(local_memory[0]);
    });
    goopax::goopax_future<int> res_a = k(arg_a,arg_b);
    std::cout << res_a.get() << " should be 102607" << std::endl;
    return global_memory_a == res_a.get() ? 0 : 1;
}