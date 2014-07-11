/***************************************************************************
 *  static_list_node_constraint.cpp - node constraint that holds a static
 *                                    of nodes to block
 *
 *  Created: Sun Mar 02 10:47:35 2014
 *  Copyright  2014  Sebastian Reuter
 *             2014  Tim Niemueller
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#include <plugins/navgraph/constraints/static_list_node_constraint.h>

#include <algorithm>

namespace fawkes{
#if 0 /* just to make Emacs auto-indent happy */
}
#endif

/** @class NavGraphStaticListNodeConstraint <plugins/navgraph/constraints/static_list_node_constraint.h>
 * Constraint that holds a list of nodes to block.
 * @author Sebastian Reuter
 */


/** Constructor.
 * @param name name of node constraint
 */
NavGraphStaticListNodeConstraint::NavGraphStaticListNodeConstraint(std::string name)
  : NavGraphNodeConstraint(name)
{
}



/** Constructor.
 * @param name name of node constraint
 * @param node_list list of nodes to block
 */
NavGraphStaticListNodeConstraint::NavGraphStaticListNodeConstraint(
    std::string name,
    std::vector<fawkes::TopologicalMapNode> &node_list)
  : NavGraphNodeConstraint(name)
{
  node_list_ = node_list;
}

/** Virtual empty destructor. */
NavGraphStaticListNodeConstraint::~NavGraphStaticListNodeConstraint()
{
}


/** Add a single node to constraint list.
 * @param node node to add to constraint list
 */
void
NavGraphStaticListNodeConstraint::add_node(const fawkes::TopologicalMapNode &node)
{
  if (! has_node(node)) {
    node_list_.push_back(node);
  }
}

/** Add multiple nodes to constraint list.
 * @param nodes nodes to add to constraint list
 */
void
NavGraphStaticListNodeConstraint::add_nodes(
  const std::vector<fawkes::TopologicalMapNode> &nodes)
{
  for (const TopologicalMapNode &n : nodes) {
    add_node(n);
  }
}

/** Remove a single node from the constraint list.
 * @param node node to remote
 */
void
NavGraphStaticListNodeConstraint::remove_node(const fawkes::TopologicalMapNode &node)
{
  if( ! node_list_.empty()) {
    std::vector<TopologicalMapNode>::iterator i;
    for( i = node_list_.begin(); i != node_list_.end(); ++i){
      if( i->name() == node.name() ){
	node_list_.erase(i);
      }
    }
  }
}

/** Check if constraint has a specific node.
 * @param node node to check
 * @return true if node is in list, false otherwise
 */
bool
NavGraphStaticListNodeConstraint::has_node(const fawkes::TopologicalMapNode &node)
{
  return (std::find(node_list_.begin(), node_list_.end(), node) != node_list_.end());
}


/** Get list of blocked nodes.
 * @return list of blocked nodes
 */
const std::vector<fawkes::TopologicalMapNode> &
NavGraphStaticListNodeConstraint::node_list() const
{
  return node_list_;
}


/** Remove all nodes. */
void
NavGraphStaticListNodeConstraint::clear_nodes()
{
  node_list_.clear();
}


} // end of namespace fawkes
