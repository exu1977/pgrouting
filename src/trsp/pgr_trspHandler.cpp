/*PGR-GNU*****************************************************************

File: pgr_trspHandler.cpp

Copyright (c) 2011 pgRouting developers
Mail: project@pgrouting.org

------

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
aint64_t with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

********************************************************************PGR-GNU*/

#include "trsp/pgr_trspHandler.h"

#include <functional>
#include <utility>
#include <queue>
#include <vector>
#include <limits>

#include "cpp_common/basePath_SSEC.hpp"
#include "cpp_common/pgr_assert.h"

namespace pgrouting {
namespace trsp {

// -------------------------------------------------------------------------
Pgr_trspHandler::Pgr_trspHandler(
        pgr_edge_t *edges,
        const size_t edge_count,
        const int64_t start_vertex, 
        const int64_t end_vertex,
        const bool directed,
        const std::vector<Rule> &ruleList) :
    m_lStartEdgeId(-1),
    m_lEndEdgeId(0),
    m_dStartpart(0.0),
    m_dEndPart(0.0),
    isStartVirtual(false),
    isEndVirtual(false),
    m_start_vertex(start_vertex),
    m_end_vertex(end_vertex),
    m_ruleTable(),
    m_bIsturnRestrictOn(false),
    m_bIsGraphConstructed(false)
{
#if 0
    init();
    initialize_restrictions(
            ruleList);
    construct_graph(edges,
            edge_count,
            directed);
#endif
    pgassert(m_bIsturnRestrictOn);
    pgassert(m_bIsGraphConstructed);
}


// -------------------------------------------------------------------------
Pgr_trspHandler::Pgr_trspHandler(void) {
    m_lStartEdgeId = -1;
    m_lEndEdgeId = 0;
    m_dStartpart = 0.0;
    m_dEndPart = 0.0;
    m_bIsturnRestrictOn = false;
    m_bIsGraphConstructed = false;
    init();
}



// -------------------------------------------------------------------------
int64_t Pgr_trspHandler::renumber_edges(
        pgr_edge_t *edges,
        size_t total_edges) const {

        int64_t v_min_id = UINT64_MAX;
        size_t z;
        for (z = 0; z < total_edges; z++) {
            if (edges[z].source < v_min_id)
                v_min_id = edges[z].source;

            if (edges[z].target < v_min_id)
                v_min_id = edges[z].target;
        }

        for (z = 0; z < total_edges; z++) {
            edges[z].source -= v_min_id;
            edges[z].target -= v_min_id;
        }

        return v_min_id;
}



// -------------------------------------------------------------------------
void Pgr_trspHandler::init() {
    max_edge_id = 0;
    max_node_id = 0;
    isStartVirtual = false;
    isEndVirtual = false;
}


// -------------------------------------------------------------------------
void Pgr_trspHandler::clear() {
    m_vecEdgeVector.clear();
    parent.clear();
    m_dCost.clear();
}


// -------------------------------------------------------------------------
double Pgr_trspHandler::construct_path(int64_t ed_id, int64_t v_pos) {
    if (parent[ed_id].ed_ind[v_pos] == -1) {
        Path_t pelement;
        auto cur_edge = &m_vecEdgeVector[ed_id];
        if (v_pos == 0) {
            pelement.node = cur_edge->startNode();
            pelement.cost = cur_edge->cost();
        } else {
            pelement.node = cur_edge->endNode();
            pelement.cost = cur_edge->r_cost();
        }
        pelement.edge = cur_edge->edgeID();

        m_vecPath.push_back(pelement);
        pgassert(m_vecPath.start_id() == m_start_vertex);
        return pelement.cost;
    }

    double ret = construct_path(parent[ed_id].ed_ind[v_pos],
        parent[ed_id].v_pos[v_pos]);
    Path_t pelement;
    auto cur_edge = &m_vecEdgeVector[ed_id];
    if (v_pos == 0) {
        pelement.node = cur_edge->startNode();
        pelement.cost = m_dCost[ed_id].endCost - ret;  // cur_edge.m_dCost;
        ret = m_dCost[ed_id].endCost;
    } else {
        pelement.node = cur_edge->endNode();
        pelement.cost = m_dCost[ed_id].startCost - ret;
        ret = m_dCost[ed_id].startCost;
    }
    pelement.edge = cur_edge->edgeID();

    m_vecPath.push_back(pelement);

    return ret;
}


// -------------------------------------------------------------------------
double Pgr_trspHandler::getRestrictionCost(
    int64_t edge_ind,
    const EdgeInfo& new_edge,
    bool isStart) {
    double cost = 0.0;
    int64_t edge_id = new_edge.edgeID();
    if (m_ruleTable.find(edge_id) == m_ruleTable.end()) {
        return(0.0);
    }
    auto vecRules = m_ruleTable[edge_id];
    int64_t st_edge_ind = edge_ind;
    for (const auto &rule : vecRules) {
        bool flag = true;
        int64_t v_pos = (isStart? 0 : 1);
        edge_ind = st_edge_ind;

        pgassert(!(edge_ind == -1));
        for (auto const &precedence : rule.precedencelist()) {
            if (precedence != m_vecEdgeVector[edge_ind].edgeID()) {
                flag = false;
                break;
            }
            auto parent_ind = parent[edge_ind].ed_ind[v_pos];
            v_pos = parent[edge_ind].v_pos[v_pos];
            edge_ind = parent_ind;
        }
        if (flag)
            cost += rule.cost();
    }
    return cost;
}


// -------------------------------------------------------------------------
void Pgr_trspHandler::explore(
    int64_t cur_node,
    const EdgeInfo cur_edge,
    bool isStart,
    const std::vector<int64_t> &vecIndex) {
    double extCost = 0.0;
    double totalCost;
    for (const auto &index : vecIndex) {
        auto new_edge = &m_vecEdgeVector[index];
        extCost = 0.0;
        if (m_bIsturnRestrictOn) {
            extCost = getRestrictionCost(cur_edge.edgeIndex(),
                 *new_edge, isStart);
        }
        if (new_edge->startNode() == cur_node) {
            if (new_edge->cost() >= 0.0) {
                if (isStart)
                    totalCost = m_dCost[cur_edge.edgeIndex()].endCost +
                    new_edge->cost() + extCost;
                else
                    totalCost = m_dCost[cur_edge.edgeIndex()].startCost +
                    new_edge->cost() + extCost;
                if (totalCost < m_dCost[index].endCost) {
                    m_dCost[index].endCost = totalCost;
                    parent[new_edge->edgeIndex()].v_pos[0] = (isStart?0:1);
                    parent[new_edge->edgeIndex()].ed_ind[0] =
                     cur_edge.edgeIndex();
                    que.push(std::make_pair(totalCost,
                        std::make_pair(new_edge->edgeIndex(), true)));
                }
            }
        } else {
            if (new_edge->r_cost() >= 0.0) {
                if (isStart)
                    totalCost = m_dCost[cur_edge.edgeIndex()].endCost +
                    new_edge->r_cost() + extCost;
                else
                    totalCost = m_dCost[cur_edge.edgeIndex()].startCost +
                    new_edge->r_cost() + extCost;
                if (totalCost < m_dCost[index].startCost) {
                    m_dCost[index].startCost = totalCost;
                    parent[new_edge->edgeIndex()].v_pos[1] = (isStart?0:1);
                    parent[new_edge->edgeIndex()].ed_ind[1] =
                    cur_edge.edgeIndex();
                    que.push(std::make_pair(totalCost,
                        std::make_pair(new_edge->edgeIndex(), false)));
                }
            }
        }
    }
}



// -------------------------------------------------------------------------
int Pgr_trspHandler::my_dijkstra1(int64_t start_vertex, int64_t end_vertex,
    size_t edge_count, char **err_msg) {
    if (!m_bIsGraphConstructed) {
        *err_msg = (char *)"Graph not Ready!";
        return -1;
    }
    unsigned int i;
    for (i = 0; i <= edge_count; i++) {
        m_dCost[i].startCost = 1e15;
        m_dCost[i].endCost = 1e15;
    }

    if (m_mapNodeId2Edge.find(start_vertex) == m_mapNodeId2Edge.end()) {
        *err_msg = (char *)"Source Not Found";
        clear();
        return -1;
    }

    if (m_mapNodeId2Edge.find(end_vertex) == m_mapNodeId2Edge.end()) {
        *err_msg = (char *)"Destination Not Found";
        clear();
        return -1;
    }

    std::priority_queue<PDP, std::vector<PDP>, std::greater<PDP> > que;
    auto vecsource = m_mapNodeId2Edge[start_vertex];
    EdgeInfo* cur_edge = NULL;

    for (const auto &source : vecsource) {
        auto cur_edge = &m_vecEdgeVector[source];
        if (cur_edge->startNode() == m_start_vertex) {
            if (cur_edge->cost() >= 0.0) {
                m_dCost[cur_edge->edgeIndex()].endCost = cur_edge->cost();
                parent[cur_edge->edgeIndex()].v_pos[0] = -1;
                parent[cur_edge->edgeIndex()].ed_ind[0] = -1;
                que.push(std::make_pair(cur_edge->cost(),
                    std::make_pair(cur_edge->edgeIndex(), true)));
                }
            } else {
                if (cur_edge->r_cost() >= 0.0) {
                    m_dCost[cur_edge->edgeIndex()].startCost =
                    cur_edge->r_cost();
                    parent[cur_edge->edgeIndex()].v_pos[1] = -1;
                    parent[cur_edge->edgeIndex()].ed_ind[1] = -1;
                    que.push(std::make_pair(cur_edge->r_cost(),
                        std::make_pair(cur_edge->edgeIndex(), false)));
                    }
            }
        }

        int64_t cur_node(0);

        while (!que.empty()) {
            PDP cur_pos = que.top();
            que.pop();
            int64_t cured_index = cur_pos.second.first;
            auto cur_edge = &m_vecEdgeVector[cured_index];

            if (cur_pos.second.second) {  // explore edges connected to end node
                cur_node = cur_edge->endNode();
                if (cur_edge->cost() < 0.0)
                    continue;
                if (cur_node == m_end_vertex)
                    break;
                explore(cur_node, *cur_edge, true,
                    cur_edge->endConnedtedEdge());
            } else {  // explore edges connected to start node
                cur_node = cur_edge->startNode();
                if (cur_edge->r_cost() < 0.0)
                    continue;
                if (cur_node == m_end_vertex)
                    break;
                explore(cur_node, *cur_edge, false,
                    cur_edge->startConnectedEdge());
            }
        }
        if (cur_node != m_end_vertex) {
            *err_msg = (char *)"Path Not Found";
            clear();
            return -1;
        } else {
            if (cur_node == cur_edge->startNode()) {
                construct_path(cur_edge->edgeIndex(), 1);
            } else {
                construct_path(cur_edge->edgeIndex(), 0);
            }
            Path_t pelement;
            pelement.node = end_vertex;
            pelement.edge = -1;
            pelement.cost = 0.0;
            m_vecPath.push_back(pelement);
        }
    return 0;
}


// -------------------------------------------------------------------------
int Pgr_trspHandler::initialize_restrictions(
        const std::vector<Rule> &ruleList) {

    for (const auto rule : ruleList) {
        auto dest_edge_id = rule.dest_id();
        if (m_ruleTable.find(dest_edge_id) != m_ruleTable.end()) {
            m_ruleTable[dest_edge_id].push_back(rule);
        } else {
            std::vector<Rule> r;
            r.push_back(rule);
            m_ruleTable.insert(std::make_pair(dest_edge_id, r));
        }
    }

    m_bIsturnRestrictOn = true;
    return true;
}

/** initializeAndProcess
 *
 * Acts as initializer and processing
 *
 */
Path
Pgr_trspHandler::initializeAndProcess(
        pgr_edge_t *edges,
        size_t edge_count,

        const std::vector<Rule> &ruleList,

        const int64_t start_vertex,
        const int64_t end_vertex,

        bool directed,
        char **err_msg) {
    /*
     * Preconditions
     */
    pgassert(!m_bIsturnRestrictOn);

    m_start_vertex = start_vertex;
    m_end_vertex = end_vertex;

    initialize_restrictions(ruleList);

    m_min_id = renumber_edges(edges, edge_count);
    m_start_vertex -= m_min_id;
    m_end_vertex -= m_min_id;
    
    Path tmp(m_start_vertex, m_end_vertex);
    m_vecPath = tmp;

    construct_graph(edges, edge_count, directed);

    if (m_mapNodeId2Edge.find(m_start_vertex) == m_mapNodeId2Edge.end()) {
        *err_msg = (char *)"Source Not Found";
        clear();
        return Path();
    }

    if (m_mapNodeId2Edge.find(m_end_vertex) == m_mapNodeId2Edge.end()) {
        *err_msg = (char *)"Destination Not Found";
        clear();
        return Path();
    }

    /*
     * Post-conditions
     */
    pgassert(m_bIsGraphConstructed);
    pgassert(m_bIsturnRestrictOn);

    return process_trsp(
            edge_count,
            err_msg);
}







// -------------------------------------------------------------------------

void  Pgr_trspHandler::initialize_que() {

    /*
     * For each adjacent edge to the start_vertex
     */
    for (const auto &source : m_mapNodeId2Edge[m_start_vertex]) {
        auto cur_edge = &m_vecEdgeVector[source];
        if (cur_edge->startNode() == m_start_vertex) {
            if (cur_edge->cost() >= 0.0) {
                m_dCost[cur_edge->edgeIndex()].endCost = cur_edge->cost();
                parent[cur_edge->edgeIndex()].v_pos[0] = -1;
                parent[cur_edge->edgeIndex()].ed_ind[0] = -1;
                que.push(std::make_pair(cur_edge->cost(),
                            std::make_pair(cur_edge->edgeIndex(), true)));
            }
        } else {
            if (cur_edge->r_cost() >= 0.0) {
                m_dCost[cur_edge->edgeIndex()].startCost =
                    cur_edge->r_cost();
                parent[cur_edge->edgeIndex()].v_pos[1] = -1;
                parent[cur_edge->edgeIndex()].ed_ind[1] = -1;
                que.push(std::make_pair(cur_edge->r_cost(),
                            std::make_pair(cur_edge->edgeIndex(), false)));
            }
        }
    }
}

EdgeInfo* Pgr_trspHandler::dijkstra_exploration(
        EdgeInfo* cur_edge,
        int64_t  &cur_node) {
    while (!que.empty()) {
        auto cur_pos = que.top();
        que.pop();

        auto cured_index = cur_pos.second.first;
        cur_edge = &m_vecEdgeVector[cured_index];

        if (cur_pos.second.second) {
            /*
             * explore edges connected to end node
             */
            cur_node = cur_edge->endNode();
            if (cur_edge->cost() < 0.0) continue;
            if (cur_node == m_end_vertex) break;
            explore(cur_node, *cur_edge, true, cur_edge->endConnedtedEdge());
        } else {
            /*
             *  explore edges connected to start node
             */
            cur_node = cur_edge->startNode();
            if (cur_edge->r_cost() < 0.0) continue;
            if (cur_node == m_end_vertex) break;
            explore(cur_node, *cur_edge, false, cur_edge->startConnectedEdge());
        }
    }
    return cur_edge;
}




Path
Pgr_trspHandler::process_trsp(
        size_t edge_count,
        char **err_msg) {
    pgassert(m_bIsturnRestrictOn);
    pgassert(m_bIsGraphConstructed);
    pgassert(m_vecPath.start_id() == m_start_vertex);
    pgassert(m_vecPath.end_id() == m_end_vertex);

    parent.resize(edge_count +1);
    m_dCost.resize(edge_count + 1);

    initialize_que();

    EdgeInfo* cur_edge = NULL;
    int64_t cur_node;

    pgassert(m_vecPath.start_id() == m_start_vertex);

    cur_edge = dijkstra_exploration(cur_edge, cur_node);

    pgassert(m_vecPath.start_id() == m_start_vertex);
    if (cur_node != m_end_vertex) {
        Path result;
        if (m_lStartEdgeId == m_lEndEdgeId) {
            pgassert(m_vecPath.start_id() == m_start_vertex);
            result = get_single_cost(1000.0).renumber_vertices(m_min_id);
        }
        *err_msg = (char *)"Path Not Found";
        clear();
        return result;
    } 
    
    double total_cost;
    pgassert(m_vecPath.start_id() == m_start_vertex);

    if (cur_node == cur_edge->startNode()) {
        total_cost = m_dCost[cur_edge->edgeIndex()].startCost;
        construct_path(cur_edge->edgeIndex(), 1);
    } else {
        total_cost = m_dCost[cur_edge->edgeIndex()].endCost;
        construct_path(cur_edge->edgeIndex(), 0);
    }

    Path_t pelement;
    pelement.node = m_end_vertex;
    pelement.edge = -1;
    pelement.cost = 0.0;
    m_vecPath.push_back(pelement);

    if (m_lStartEdgeId == m_lEndEdgeId) {
        return get_single_cost(total_cost).renumber_vertices(m_min_id);
    }

    clear();
    m_vecPath.Path::recalculate_agg_cost();
    return m_vecPath.renumber_vertices(m_min_id);
}




// -------------------------------------------------------------------------
Path
Pgr_trspHandler::get_single_cost(
        double total_cost) {
    auto start_edge_info =
        &m_vecEdgeVector[m_mapEdgeId2Index[m_lStartEdgeId]];
    if (m_dEndPart >= m_dStartpart) {
        if (start_edge_info->cost() >= 0.0 && start_edge_info->cost() *
                (m_dEndPart - m_dStartpart) <= total_cost) {
#if 0
            *path = (path_element_tt *) malloc(sizeof(path_element_tt) * (1));
            *path_count = 1;
            (*path)[0].vertex_id = -1;
            (*path)[0].edge_id = m_lStartEdgeId;
            (*path)[0].cost = start_edge_info->cost() *
                (m_dEndPart - m_dStartpart);
#endif
            return m_vecPath.renumber_vertices(m_min_id);
        }
    } else {
        if (start_edge_info->r_cost() >= 0.0 &&
                start_edge_info->r_cost() * (m_dStartpart - m_dEndPart) <=
                total_cost) {
#if 0
            *path = (path_element_tt *) malloc(sizeof(path_element_tt) * (1));
            *path_count = 1;
            (*path)[0].vertex_id = -1;
            (*path)[0].edge_id = m_lStartEdgeId;
            (*path)[0].cost = start_edge_info->r_cost() *
                (m_dStartpart - m_dEndPart);
#endif
            return m_vecPath.renumber_vertices(m_min_id);
        }
    }
    return m_vecPath.renumber_vertices(m_min_id);
}


// -------------------------------------------------------------------------
bool Pgr_trspHandler::construct_graph(
        pgr_edge_t* edges,
        const size_t edge_count,
        const bool directed) {
    pgassert(!m_bIsGraphConstructed);

    init();
    for (size_t i = 0; i < edge_count; i++) {
        auto current_edge = &edges[i];

        /*
         * making all costs > 0
         */
        if (current_edge->cost < 0 && current_edge->reverse_cost > 0) {
            std::swap(current_edge->cost, current_edge->reverse_cost);
            std::swap(current_edge->source, current_edge->target);
        }

        if (!directed) {
            if (current_edge->reverse_cost < 0) {
                current_edge->reverse_cost = current_edge->cost;
            }
        }
        addEdge(*current_edge);
    }
    m_bIsGraphConstructed = true;
    return true;
}


// -------------------------------------------------------------------------
bool Pgr_trspHandler::connectEdge(EdgeInfo& firstEdge,
        EdgeInfo& secondEdge, bool bIsStartNodeSame) {
    if (bIsStartNodeSame) {
        if (firstEdge.r_cost() >= 0.0)
            firstEdge.startConnectedEdge().push_back(
                    secondEdge.edgeIndex());
        if (firstEdge.startNode() == secondEdge.startNode()) {
            if (secondEdge.r_cost() >= 0.0)
                secondEdge.startConnectedEdge().push_back(
                        firstEdge.edgeIndex());
        } else {
            if (secondEdge.cost() >= 0.0)
                secondEdge.endConnedtedEdge().push_back(
                        firstEdge.edgeIndex());
        }
    } else {
        if (firstEdge.cost() >= 0.0)
            firstEdge.endConnedtedEdge().push_back(secondEdge.edgeIndex());
        if (firstEdge.endNode() == secondEdge.startNode()) {
            if (secondEdge.r_cost() >= 0.0)
                secondEdge.startConnectedEdge().push_back(
                        firstEdge.edgeIndex());
        } else {
            if (secondEdge.cost() >= 0.0)
                secondEdge.endConnedtedEdge().push_back(
                        firstEdge.edgeIndex());
        }
    }
    return true;
}


// -------------------------------------------------------------------------
bool Pgr_trspHandler::addEdge(const pgr_edge_t edgeIn) {
    // int64_t lTest;
    auto itMap = m_mapEdgeId2Index.find(edgeIn.id);
    if (itMap != m_mapEdgeId2Index.end())
        return false;


    EdgeInfo newEdge(edgeIn, m_vecEdgeVector.size());

    if (edgeIn.id > max_edge_id) {
        max_edge_id = static_cast<size_t>(edgeIn.id);
    }

    if (newEdge.startNode() > max_node_id) {
        max_node_id = newEdge.startNode();
    }
    if (newEdge.endNode() > max_node_id) {
        max_node_id = newEdge.endNode();
    }

    // Searching the start node for connectivity
    auto itNodeMap = m_mapNodeId2Edge.find(
            edgeIn.source);
    if (itNodeMap != m_mapNodeId2Edge.end()) {
        // Connect current edge with existing edge with start node
        // connectEdge(
        int64_t lEdgeCount = itNodeMap->second.size();
        int64_t lEdgeIndex;
        for (lEdgeIndex = 0; lEdgeIndex < lEdgeCount; lEdgeIndex++) {
            int64_t lEdge = itNodeMap->second.at(lEdgeIndex);
            connectEdge(newEdge, m_vecEdgeVector[lEdge], true);
        }
    }


    // Searching the end node for connectivity
    itNodeMap = m_mapNodeId2Edge.find(edgeIn.target);
    if (itNodeMap != m_mapNodeId2Edge.end()) {
        // Connect current edge with existing edge with end node
        // connectEdge(
        int64_t lEdgeCount = itNodeMap->second.size();
        int64_t lEdgeIndex;
        for (lEdgeIndex = 0; lEdgeIndex < lEdgeCount; lEdgeIndex++) {
            int64_t lEdge = itNodeMap->second.at(lEdgeIndex);
            connectEdge(newEdge, m_vecEdgeVector[lEdge], false);
        }
    }



    // Add this node and edge into the data structure
    m_mapNodeId2Edge[edgeIn.source].push_back(newEdge.edgeIndex());
    m_mapNodeId2Edge[edgeIn.target].push_back(newEdge.edgeIndex());


    // Adding edge to the list
    m_mapEdgeId2Index.insert(std::make_pair(newEdge.edgeID(),
                m_vecEdgeVector.size()));
    m_vecEdgeVector.push_back(newEdge);

    //


    return true;
}



}  // namespace trsp  
}  // namespace pgrouting
