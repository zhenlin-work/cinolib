/*********************************************************************************
*  Copyright(C) 2016: Marco Livesu                                               *
*  All rights reserved.                                                          *
*                                                                                *
*  This file is part of CinoLib                                                  *
*                                                                                *
*  CinoLib is dual-licensed:                                                     *
*                                                                                *
*   - For non-commercial use you can redistribute it and/or modify it under the  *
*     terms of the GNU General Public License as published by the Free Software  *
*     Foundation; either version 3 of the License, or (at your option) any later *
*     version.                                                                   *
*                                                                                *
*   - If you wish to use it as part of a commercial software, a proper agreement *
*     with the Author(s) must be reached, based on a proper licensing contract.  *
*                                                                                *
*  This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE       *
*  WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.     *
*                                                                                *
*  Author(s):                                                                    *
*                                                                                *
*     Marco Livesu (marco.livesu@gmail.com)                                      *
*     http://pers.ge.imati.cnr.it/livesu/                                        *
*                                                                                *
*     Italian National Research Council (CNR)                                    *
*     Institute for Applied Mathematics and Information Technologies (IMATI)     *
*     Via de Marini, 6                                                           *
*     16149 Genoa,                                                               *
*     Italy                                                                      *
**********************************************************************************/
#include <cinolib/meshes/abstract_polygonmesh.h>
#include <cinolib/io/read_write.h>
#include <cinolib/quality.h>

#include <unordered_set>

namespace cinolib
{

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::load(const char * filename)
{
    this->clear();

    std::string str(filename);
    std::string filetype = str.substr(str.size()-4,4);

    if (filetype.compare(".off") == 0 ||
        filetype.compare(".OFF") == 0)
    {
        read_OFF(filename, this->verts, this->polys);
    }
    else if (filetype.compare(".obj") == 0 ||
             filetype.compare(".OBJ") == 0)
    {
        read_OBJ(filename, this->verts, this->polys);
    }
    else
    {
        std::cerr << "ERROR : " << __FILE__ << ", line " << __LINE__ << " : load() : file format not supported yet " << endl;
        exit(-1);
    }

    this->mesh_data().filename = std::string(filename);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::save(const char * filename) const
{
    std::vector<double> coords = serialized_xyz_from_vec3d(this->verts);

    std::string str(filename);
    std::string filetype = str.substr(str.size()-3,3);

    if (filetype.compare("off") == 0 ||
        filetype.compare("OFF") == 0)
    {
        write_OFF(filename, coords, this->polys);
    }
    else if (filetype.compare("obj") == 0 ||
             filetype.compare("OBJ") == 0)
    {
        write_OBJ(filename, coords, this->polys);
    }
    else
    {
        std::cerr << "ERROR : " << __FILE__ << ", line " << __LINE__ << " : write() : file format not supported yet " << endl;
        exit(-1);
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::init()
{
    this->update_adjacency();
    this->update_bbox();

    this->v_data.resize(this->num_verts());
    this->e_data.resize(this->num_edges());
    this->p_data.resize(this->num_polys());

    this->update_normals();

    this->copy_xyz_to_uvw(UVW_param);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::update_adjacency()
{
    this->v2v.clear(); this->v2v.resize(this->num_verts());
    this->v2e.clear(); this->v2e.resize(this->num_verts());
    this->v2p.clear(); this->v2p.resize(this->num_verts());
    this->p2p.clear(); this->p2p.resize(this->num_polys());
    this->p2e.clear(); this->p2e.resize(this->num_polys());

    std::map<ipair,std::vector<uint>> e2f_map;
    for(uint pid=0; pid<this->num_polys(); ++pid)
    {
        for(uint offset=0; offset<this->verts_per_poly(pid); ++offset)
        {
            uint vid0 = this->poly_vert_id(pid,offset);
            uint vid1 = this->poly_vert_id(pid,(offset+1)%this->verts_per_poly(pid));
            this->v2p.at(vid0).push_back(pid);
            e2f_map[unique_pair(vid0,vid1)].push_back(pid);
        }
    }

    this->edges.clear();
    this->e2p.clear();
    this->e2p.resize(e2f_map.size());

    uint fresh_id = 0;
    for(auto e2f_it : e2f_map)
    {
        ipair e    = e2f_it.first;
        uint  eid  = fresh_id++;
        uint  vid0 = e.first;
        uint  vid1 = e.second;

        this->edges.push_back(vid0);
        this->edges.push_back(vid1);

        this->v2v.at(vid0).push_back(vid1);
        this->v2v.at(vid1).push_back(vid0);

        this->v2e.at(vid0).push_back(eid);
        this->v2e.at(vid1).push_back(eid);

        std::vector<uint> pids = e2f_it.second;
        for(uint pid : pids)
        {
            this->p2e.at(pid).push_back(eid);
            this->e2p.at(eid).push_back(pid);
            for(uint adj_pid : pids) if (pid != adj_pid) this->p2p.at(pid).push_back(adj_pid);
        }

        // MANIFOLDNESS CHECKS
        //
        bool is_manifold = (pids.size() > 2 || pids.size() < 1);
        if (is_manifold && !support_non_manifold_edges)
        {
            std::cerr << "Non manifold edge found! To support non manifoldness,";
            std::cerr << "enable the 'support_non_manifold_edges' flag in cinolib.h" << endl;
            assert(false);
        }
        if (is_manifold && print_non_manifold_edges)
        {
            std::cerr << "Non manifold edge! (" << vid0 << "," << vid1 << ")" << endl;
        }
    }

    logger << this->num_verts() << "\tverts" << endl;
    logger << this->num_polys() << "\tfaces" << endl;
    logger << this->num_edges() << "\tedges" << endl;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::update_v_normal(const uint vid)
{
    vec3d n(0,0,0);
    for(uint pid : this->adj_v2p(vid))
    {
        n += this->poly_data(pid).normal;
    }
    n.normalize();
    this->vert_data(vid).normal = n;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::update_p_normals()
{
    for(uint pid=0; pid<this->num_polys(); ++pid)
    {
        update_p_normal(pid);
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::update_v_normals()
{
    for(uint vid=0; vid<this->num_verts(); ++vid)
    {
        update_v_normal(vid);
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::update_normals()
{
    this->update_p_normals();
    this->update_v_normals();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
double AbstractPolygonMesh<M,V,E,P>::mesh_volume() const
{
    // EFFICIENT FEATURE EXTRACTION FOR 2D/3D OBJECTS IN MESH REPRESENTATION
    // Cha Zhang and Tsuhan Chen
    // Proceedings of the International Conference on Image Processing, 2001

    double vol = 0.0;
    vec3d O(0,0,0);
    for(uint pid=0; pid<this->num_polys(); ++pid)
    {
        for(uint i=0; i<this->poly_tessellation(pid).size()/3; ++i)
        {
            vec3d A    = this->vert(this->poly_tessellation(pid).at(3*i+0));
            vec3d B    = this->vert(this->poly_tessellation(pid).at(3*i+1));
            vec3d C    = this->vert(this->poly_tessellation(pid).at(3*i+2));

            vec3d OA   = A - O;
            vec3d n    = this->poly_data(pid).normal;

            vol += (n.dot(OA) > 0) ?  tet_unsigned_volume(A,B,C,O)
                                   : -tet_unsigned_volume(A,B,C,O);
        }
    }
    assert(vol >= 0);
    return vol;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::vert_is_saddle(const uint vid, const int tex_coord) const
{
    std::vector<bool> signs;
    for(uint nbr : vert_ordered_vert_ring(vid))
    {
        // Discard == signs. For references, see:
        // Decomposing Polygon Meshes by Means of Critical Points
        // Yinan Zhou and Zhiyong Huang
        //
        switch (tex_coord)
        {
            case U_param : if (this->vert_data(nbr).uvw[0] != this->vert_data(vid).uvw[0]) signs.push_back(this->vert_data(nbr).uvw[0] > this->vert_data(vid).uvw[0]); break;
            case V_param : if (this->vert_data(nbr).uvw[1] != this->vert_data(vid).uvw[1]) signs.push_back(this->vert_data(nbr).uvw[1] > this->vert_data(vid).uvw[1]); break;
            case W_param : if (this->vert_data(nbr).uvw[2] != this->vert_data(vid).uvw[2]) signs.push_back(this->vert_data(nbr).uvw[2] > this->vert_data(vid).uvw[2]); break;
            default: assert(false);
        }
    }

    uint sign_switch = 0;
    for(uint i=0; i<signs.size(); ++i)
    {
        if (signs.at(i) != signs.at((i+1)%signs.size())) ++sign_switch;
    }

    if (sign_switch > 2) return true;
    return false;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::vert_is_critical_p(const uint vid, const int tex_coord) const
{
    return (this->vert_is_local_max(vid,tex_coord) ||
            this->vert_is_local_min(vid,tex_coord) ||
            this->vert_is_saddle   (vid,tex_coord));
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::poly_verts_are_CCW(const uint pid, const uint curr, const uint prev) const
{
    uint prev_offset = this->poly_vert_offset(pid, prev);
    uint curr_offset = this->poly_vert_offset(pid, curr);
    if (curr_offset == (prev_offset+1)%this->verts_per_poly(pid)) return true;
    return false;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::vert_ordered_one_ring(const uint vid,
                                                         std::vector<uint> & v_ring,       // sorted list of adjacent vertices
                                                         std::vector<uint> & p_ring,       // sorted list of adjacent triangles
                                                         std::vector<uint> & e_ring,       // sorted list of edges incident to vid
                                                         std::vector<uint> & e_link) const // sorted list of edges opposite to vid
{
    v_ring.clear();
    p_ring.clear();
    e_ring.clear();
    e_link.clear();

    if (this->adj_v2e(vid).empty()) return;
    uint curr_e  = this->adj_v2e(vid).front(); assert(edge_is_manifold(curr_e));
    uint curr_v  = this->vert_opposite_to(curr_e, vid);
    uint curr_p  = this->adj_e2p(curr_e).front();
    // impose CCW winding...
    if (!this->poly_verts_are_CCW(curr_p, curr_v, vid)) curr_p = this->adj_e2p(curr_e).back();

    // If there are boundary edges it is important to start from the right triangle (i.e. right-most),
    // otherwise it will be impossible to cover the entire umbrella
    std::vector<uint> b_edges = vert_boundary_edges(vid);
    if (b_edges.size()  > 0)
    {
        assert(b_edges.size() == 2); // otherwise there is no way to cover the whole umbrella walking through adjacent triangles!!!

        uint e = b_edges.front();
        uint p = this->adj_e2p(e).front();
        uint v = this->vert_opposite_to(e, vid);

        if (!this->poly_verts_are_CCW(p, v, vid))
        {
            e = b_edges.back();
            p = this->adj_e2p(e).front();
            v = this->vert_opposite_to(e, vid);
            assert(this->poly_verts_are_CCW(p, v, vid));
        }

        curr_e = e;
        curr_p = p;
        curr_v = v;
    }

    do
    {
        e_ring.push_back(curr_e);
        p_ring.push_back(curr_p);

        uint off = this->poly_vert_offset(curr_p, curr_v);
        for(uint i=0; i<this->verts_per_poly(curr_p)-1; ++i)
        {
            curr_v = this->poly_vert_id(curr_p,(off+i)%this->verts_per_poly(curr_p));
            if (i>0) e_link.push_back( this->poly_edge_id(curr_p, curr_v, v_ring.back()) );
            v_ring.push_back(curr_v);
        }

        curr_e = this->poly_edge_id(curr_p, vid, v_ring.back()); assert(edge_is_manifold(curr_e));
        curr_p = (this->adj_e2p(curr_e).front() == curr_p) ? this->adj_e2p(curr_e).back() : this->adj_e2p(curr_e).front();

        v_ring.pop_back();

        if (edge_is_boundary(curr_e)) e_ring.push_back(curr_e);
    }
    while(e_ring.size() < this->adj_v2e(vid).size());
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::vert_ordered_vert_ring(const uint vid) const
{
    std::vector<uint> v_ring; // sorted list of adjacent vertices
    std::vector<uint> f_ring; // sorted list of adjacent triangles
    std::vector<uint> e_ring; // sorted list of edges incident to vid
    std::vector<uint> e_link; // sorted list of edges opposite to vid
    vert_ordered_one_ring(vid, v_ring, f_ring, e_ring, e_link);
    return v_ring;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::vert_ordered_poly_ring(const uint vid) const
{
    std::vector<uint> v_ring; // sorted list of adjacent vertices
    std::vector<uint> f_ring; // sorted list of adjacent triangles
    std::vector<uint> e_ring; // sorted list of edges incident to vid
    std::vector<uint> e_link; // sorted list of edges opposite to vid
    vert_ordered_one_ring(vid, v_ring, f_ring, e_ring, e_link);
    return f_ring;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::vert_ordered_edge_ring(const uint vid) const
{
    std::vector<uint> v_ring; // sorted list of adjacent vertices
    std::vector<uint> f_ring; // sorted list of adjacent triangles
    std::vector<uint> e_ring; // sorted list of edges incident to vid
    std::vector<uint> e_link; // sorted list of edges opposite to vid
    vert_ordered_one_ring(vid, v_ring, f_ring, e_ring, e_link);
    return e_ring;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::vert_ordered_edge_link(const uint vid) const
{
    std::vector<uint> v_ring; // sorted list of adjacent vertices
    std::vector<uint> f_ring; // sorted list of adjacent triangles
    std::vector<uint> e_ring; // sorted list of edges incident to vid
    std::vector<uint> e_link; // sorted list of edges opposite to vid
    vert_ordered_one_ring(vid, v_ring, f_ring, e_ring, e_link);
    return e_link;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
double AbstractPolygonMesh<M,V,E,P>::vert_area(const uint vid) const
{
    double area = 0.0;
    for(uint pid : this->adj_v2p(vid)) area += poly_area(pid)/static_cast<double>(this->verts_per_poly(pid));
    return area;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
double AbstractPolygonMesh<M,V,E,P>::vert_mass(const uint vid) const
{
    return vert_area(vid);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::vert_is_boundary(const uint vid) const
{
    for(uint eid : this->adj_v2e(vid)) if (edge_is_boundary(eid)) return true;
    return false;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::vert_boundary_edges(const uint vid) const
{
    std::vector<uint> b_edges;
    for(uint eid : this->adj_v2e(vid)) if (edge_is_boundary(eid)) b_edges.push_back(eid);
    return b_edges;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
uint AbstractPolygonMesh<M,V,E,P>::vert_add(const vec3d & pos)
{
    uint vid = this->num_verts();
    //
    this->verts.push_back(pos);
    //
    V data;
    this->v_data.push_back(data);
    //
    this->v2v.push_back(std::vector<uint>());
    this->v2e.push_back(std::vector<uint>());
    this->v2p.push_back(std::vector<uint>());
    //
    this->bb.min = this->bb.min.min(pos);
    this->bb.max = this->bb.max.max(pos);
    //
    return vid;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::vert_switch_id(const uint vid0, const uint vid1)
{
    // [28 Aug 2017] Tested on 10K random id switches : PASSED

    if (vid0 == vid1) return;

    std::swap(this->verts.at(vid0),  this->verts.at(vid1));
    std::swap(this->v_data.at(vid0), this->v_data.at(vid1));
    std::swap(this->v2v.at(vid0),    this->v2v.at(vid1));
    std::swap(this->v2e.at(vid0),    this->v2e.at(vid1));
    std::swap(this->v2p.at(vid0),    this->v2p.at(vid1));

    std::unordered_set<uint> verts_to_update;
    verts_to_update.insert(this->adj_v2v(vid0).begin(), this->adj_v2v(vid0).end());
    verts_to_update.insert(this->adj_v2v(vid1).begin(), this->adj_v2v(vid1).end());

    std::unordered_set<uint> edges_to_update;
    edges_to_update.insert(this->adj_v2e(vid0).begin(), this->adj_v2e(vid0).end());
    edges_to_update.insert(this->adj_v2e(vid1).begin(), this->adj_v2e(vid1).end());

    std::unordered_set<uint> polys_to_update;
    polys_to_update.insert(this->adj_v2p(vid0).begin(), this->adj_v2p(vid0).end());
    polys_to_update.insert(this->adj_v2p(vid1).begin(), this->adj_v2p(vid1).end());

    for(uint nbr : verts_to_update)
    {
        for(uint & vid : this->v2v.at(nbr))
        {
            if (vid == vid0) vid = vid1; else
            if (vid == vid1) vid = vid0;
        }
    }

    for(uint eid : edges_to_update)
    {
        for(uint i=0; i<2; ++i)
        {
            uint & vid = this->edges.at(2*eid+i);
            if (vid == vid0) vid = vid1; else
            if (vid == vid1) vid = vid0;
        }
    }

    for(uint pid : polys_to_update)
    {
        for(uint & vid : this->polys.at(pid))
        {
            if (vid == vid0) vid = vid1; else
            if (vid == vid1) vid = vid0;
        }
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::vert_remove(const uint vid)
{
    polys_remove(this->adj_v2p(vid));
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::vert_remove_unreferenced(const uint vid)
{
    this->v2v.at(vid).clear();
    this->v2e.at(vid).clear();
    this->v2p.at(vid).clear();
    vert_switch_id(vid, this->num_verts()-1);
    this->verts.pop_back();
    this->v_data.pop_back();
    this->v2v.pop_back();
    this->v2e.pop_back();
    this->v2p.pop_back();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::edge_is_manifold(const uint eid) const
{
    return (this->edge_valence(eid) <= 2);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::edge_is_boundary(const uint eid) const
{
    return (this->edge_valence(eid) == 1);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::edges_share_poly(const uint eid1, const uint eid2) const
{
    for(uint pid1 : this->adj_e2p(eid1))
    for(uint pid2 : this->adj_e2p(eid2))
    {
        if (pid1 == pid2) return true;
    }
    return false;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
ipair AbstractPolygonMesh<M,V,E,P>::edge_shared(const uint pid0, const uint pid1) const
{
    std::vector<uint> shared_verts;
    uint v0 = this->poly_vert_id(pid0,0);
    uint v1 = this->poly_vert_id(pid0,1);
    uint v2 = this->poly_vert_id(pid0,2);

    if (this->poly_contains_vert(pid1,v0)) shared_verts.push_back(v0);
    if (this->poly_contains_vert(pid1,v1)) shared_verts.push_back(v1);
    if (this->poly_contains_vert(pid1,v2)) shared_verts.push_back(v2);
    assert(shared_verts.size() == 2);

    ipair e;
    e.first  = shared_verts.front();
    e.second = shared_verts.back();
    return e;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
uint AbstractPolygonMesh<M,V,E,P>::edge_add(const uint vid0, const uint vid1)
{
    assert(vid0 < this->num_verts());
    assert(vid1 < this->num_verts());
    assert(!this->verts_are_adjacent(vid0, vid1));
    assert(DOES_NOT_CONTAIN_VEC(this->v2v.at(vid0), vid1));
    assert(DOES_NOT_CONTAIN_VEC(this->v2v.at(vid1), vid0));
    assert(this->edge_id(vid0, vid1) == -1);
    //
    uint eid = this->num_edges();
    //
    this->edges.push_back(vid0);
    this->edges.push_back(vid1);
    //
    this->e2p.push_back(std::vector<uint>());
    //
    E data;
    this->e_data.push_back(data);
    //
    this->v2v.at(vid1).push_back(vid0);
    this->v2v.at(vid0).push_back(vid1);
    //
    this->v2e.at(vid0).push_back(eid);
    this->v2e.at(vid1).push_back(eid);
    //
    return eid;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::edge_switch_id(const uint eid0, const uint eid1)
{
    // [28 Aug 2017] Tested on 10K random id switches : PASSED

    if (eid0 == eid1) return;

    for(uint off=0; off<2; ++off) std::swap(this->edges.at(2*eid0+off), this->edges.at(2*eid1+off));

    std::swap(this->e2p.at(eid0),    this->e2p.at(eid1));
    std::swap(this->e_data.at(eid0), this->e_data.at(eid1));

    std::unordered_set<uint> verts_to_update;
    verts_to_update.insert(this->edge_vert_id(eid0,0));
    verts_to_update.insert(this->edge_vert_id(eid0,1));
    verts_to_update.insert(this->edge_vert_id(eid1,0));
    verts_to_update.insert(this->edge_vert_id(eid1,1));

    std::unordered_set<uint> polys_to_update;
    polys_to_update.insert(this->adj_e2p(eid0).begin(), this->adj_e2p(eid0).end());
    polys_to_update.insert(this->adj_e2p(eid1).begin(), this->adj_e2p(eid1).end());

    for(uint vid : verts_to_update)
    {
        for(uint & eid : this->v2e.at(vid))
        {
            if (eid == eid0) eid = eid1; else
            if (eid == eid1) eid = eid0;
        }
    }

    for(uint pid : polys_to_update)
    {
        for(uint & eid : this->p2e.at(pid))
        {
            if (eid == eid0) eid = eid1; else
            if (eid == eid1) eid = eid0;
        }
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::edge_remove(const uint eid)
{
    polys_remove(this->adj_e2p(eid));
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::edge_remove_unreferenced(const uint eid)
{
    this->e2p.at(eid).clear();
    edge_switch_id(eid, this->num_edges()-1);
    this->edges.resize(this->edges.size()-2);
    this->e_data.pop_back();
    this->e2p.pop_back();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::edge_mark_labeling_boundaries()
{
    for(uint eid=0; eid<this->num_edges(); ++eid)
    {
        std::set<int> unique_labels;
        for(uint pid : this->adj_e2p(eid)) unique_labels.insert(this->poly_data(pid).label);

        this->edge_data(eid).marked = (unique_labels.size()>=2);
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::edge_mark_boundaries()
{
    for(uint eid=0; eid<this->num_edges(); ++eid)
    {
        this->edge_data(eid).marked = edge_is_boundary(eid);
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
uint AbstractPolygonMesh<M,V,E,P>::poly_vert_offset(const uint pid, const uint vid) const
{
    for(uint offset=0; offset<verts_per_poly(pid); ++offset)
    {
        if (this->poly_vert_id(pid,offset) == vid) return offset;
    }
    assert(false && "Something is off here...");
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
double AbstractPolygonMesh<M,V,E,P>::poly_angle_at_vert(const uint pid, const uint vid, const int unit) const
{
    assert(this->poly_contains_vert(pid,vid));

    uint offset = 0;
    for(uint i=0; i<this->verts_per_poly(pid); ++i) if (this->poly_vert_id(pid,i) == vid) offset = i;
    assert(this->poly_vert_id(pid,offset) == vid);
    //
    // the code above substitutes this one (which was specific for AbstractMeshes...)
    //
    //     if (poly_vert_id(pid,0) == vid) offset = 0;
    //else if (poly_vert_id(pid,1) == vid) offset = 1;
    //else if (poly_vert_id(pid,2) == vid) offset = 2;
    //else { assert(false); offset=0; } // offset=0 kills uninitialized warning message

    vec3d p = this->vert(vid);
    vec3d u = this->poly_vert(pid,(offset+1)%this->verts_per_poly(pid)) - p;
    vec3d v = this->poly_vert(pid,(offset+2)%this->verts_per_poly(pid)) - p;

    switch (unit)
    {
        case RAD : return u.angle_rad(v);
        case DEG : return u.angle_deg(v);
        default  : assert(false);
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
int AbstractPolygonMesh<M,V,E,P>::poly_shared(const uint eid0, const uint eid1) const
{
    for(uint pid0 : this->adj_e2p(eid0))
    for(uint pid1 : this->adj_e2p(eid1))
    {
        if (pid0 == pid1) return pid0;
    }
    return -1;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::polys_adjacent_along(const uint pid, const uint eid) const
{
    std::vector<uint> polys;
    for(uint nbr : this->adj_p2p(pid))
    {
        if (this->poly_contains_edge(nbr,eid)) polys.push_back(nbr);
    }
    return polys;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<vec3d> AbstractPolygonMesh<M,V,E,P>::poly_vlist(const uint pid) const
{
    std::vector<vec3d> vlist(this->verts_per_poly(pid));
    for (uint i=0; i<this->verts_per_poly(pid); ++i)
    {
        vlist.at(i) = this->poly_vert(pid,i);
    }
    return vlist;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::polys_adjacent_along(const uint pid, const uint vid0, const uint vid1) const
{
    uint eid = this->poly_edge_id(pid, vid0, vid1);
    return polys_adjacent_along(pid, eid);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
int AbstractPolygonMesh<M,V,E,P>::poly_opposite_to(const uint eid, const uint pid) const
{
    assert(this->poly_contains_edge(pid,eid));
    assert(this->edge_is_manifold(eid));
    assert(!this->adj_e2p(eid).empty());

    if (this->edge_is_boundary(eid)) return -1;
    if (this->adj_e2p(eid).front() != pid) return this->adj_e2p(eid).front();
    return this->adj_e2p(eid).back();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::polys_are_adjacent(const uint pid0, const uint pid1) const
{
    for(uint eid : this->adj_p2e(pid0))
    for(uint pid : this->polys_adjacent_along(pid0, eid))
    {
        if (pid == pid1) return true;
    }
    return false;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
bool AbstractPolygonMesh<M,V,E,P>::poly_is_boundary(const uint pid) const
{
    return (this->adj_p2p(pid).size() < 3);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::poly_switch_id(const uint pid0, const uint pid1)
{
    // [28 Aug 2017] Tested on 10K random id switches : PASSED

    if (pid0 == pid1) return;

    std::swap(this->polys.at(pid0),  this->polys.at(pid1));
    std::swap(this->p_data.at(pid0), this->p_data.at(pid1));
    std::swap(this->p2e.at(pid0),    this->p2e.at(pid1));
    std::swap(this->p2p.at(pid0),    this->p2p.at(pid1));

    std::unordered_set<uint> verts_to_update;
    verts_to_update.insert(this->adj_p2v(pid0).begin(), this->adj_p2v(pid0).end());
    verts_to_update.insert(this->adj_p2v(pid1).begin(), this->adj_p2v(pid1).end());

    std::unordered_set<uint> edges_to_update;
    edges_to_update.insert(this->adj_p2e(pid0).begin(), this->adj_p2e(pid0).end());
    edges_to_update.insert(this->adj_p2e(pid1).begin(), this->adj_p2e(pid1).end());

    std::unordered_set<uint> polys_to_update;
    polys_to_update.insert(this->adj_p2p(pid0).begin(), this->adj_p2p(pid0).end());
    polys_to_update.insert(this->adj_p2p(pid1).begin(), this->adj_p2p(pid1).end());

    for(uint vid : verts_to_update)
    {
        for(uint & pid : this->v2p.at(vid))
        {
            if (pid == pid0) pid = pid1; else
            if (pid == pid1) pid = pid0;
        }
    }

    for(uint eid : edges_to_update)
    {
        for(uint & pid : this->e2p.at(eid))
        {
            if (pid == pid0) pid = pid1; else
            if (pid == pid1) pid = pid0;
        }
    }

    for(uint nbr : polys_to_update)
    {
        for(uint & pid : this->p2p.at(nbr))
        {
            if (pid == pid0) pid = pid1; else
            if (pid == pid1) pid = pid0;
        }
    }
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
uint AbstractPolygonMesh<M,V,E,P>::poly_add(const std::vector<uint> & p)
{
    for(uint vid : p) assert(vid < this->num_verts());

    uint pid = this->num_polys();
    this->polys.push_back(p);

    P data;
    this->p_data.push_back(data);

    this->p2e.push_back(std::vector<uint>());
    this->p2p.push_back(std::vector<uint>());

    // add missing edges
    for(uint i=0; i<p.size(); ++i)
    {
        uint vid0 = p.at(i);
        uint vid1 = p.at((i+1)%p.size());
        int  eid = this->edge_id(vid0, vid1);

        if (eid == -1) eid = this->edge_add(vid0, vid1);
    }

    // update connectivity
    for(uint vid : p)
    {
        this->v2p.at(vid).push_back(pid);
    }
    //
    for(uint i=0; i<p.size(); ++i)
    {
        uint vid0 = p.at(i);
        uint vid1 = p.at((i+1)%p.size());
        int  eid = this->edge_id(vid0, vid1);
        assert(eid >= 0);

        for(uint nbr : this->e2p.at(eid))
        {
            assert(nbr!=pid);
            if (this->polys_are_adjacent(pid,nbr)) continue;
            this->p2p.at(nbr).push_back(pid);
            this->p2p.at(pid).push_back(nbr);
        }

        this->e2p.at(eid).push_back(pid);
        this->p2e.at(pid).push_back(eid);
    }

    this->update_p_normal(pid);
    for(uint vid : p) this->update_v_normal(vid);

    return pid;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::polys_remove(const std::vector<uint> & pids)
{
    // in order to avoid id conflicts remove all the
    // polys starting from the one with highest id
    //
    std::vector<uint> tmp = pids;
    std::sort(tmp.begin(), tmp.end());
    std::reverse(tmp.begin(), tmp.end());
    for(uint pid : tmp) poly_remove(pid);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::poly_remove(const uint pid)
{
    // [28 Aug 2017] Tested on progressive random removal until almost no polys are left: PASSED

    std::set<uint,std::greater<uint>> dangling_verts; // higher ids first
    std::set<uint,std::greater<uint>> dangling_edges; // higher ids first

    // disconnect from vertices
    for(uint vid : this->adj_p2v(pid))
    {
        REMOVE_FROM_VEC(this->v2p.at(vid), pid);
        if (this->v2p.at(vid).empty()) dangling_verts.insert(vid);
    }

    // disconnect from edges
    for(uint eid : this->adj_p2e(pid))
    {
        REMOVE_FROM_VEC(this->e2p.at(eid), pid);
        if (this->e2p.at(eid).empty()) dangling_edges.insert(eid);
    }

    // disconnect from other polygons
    for(uint nbr : this->adj_p2p(pid)) REMOVE_FROM_VEC(this->p2p.at(nbr), pid);

    // delete dangling edges
    for(uint eid : dangling_edges)
    {
        uint vid0 = this->edge_vert_id(eid,0);
        uint vid1 = this->edge_vert_id(eid,1);
        if (vid1 > vid0) std::swap(vid0,vid1); // make sure the highest id is processed first
        REMOVE_FROM_VEC(this->v2e.at(vid0), eid);
        REMOVE_FROM_VEC(this->v2e.at(vid1), eid);
        REMOVE_FROM_VEC(this->v2v.at(vid0), vid1);
        REMOVE_FROM_VEC(this->v2v.at(vid1), vid0);
        edge_remove_unreferenced(eid);
    }

    // delete dangling vertices
    for(uint vid : dangling_verts)
    {
        assert(this->adj_v2e(vid).empty());
        for(uint nbr : this->adj_v2v(vid)) REMOVE_FROM_VEC(this->v2v.at(nbr), vid);
        vert_remove_unreferenced(vid);
    }

    poly_remove_unreferenced(pid);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::poly_remove_unreferenced(const uint pid)
{
    this->polys.at(pid).clear();
    this->p2e.at(pid).clear();
    this->p2p.at(pid).clear();
    poly_switch_id(pid, this->num_polys()-1);
    this->polys.pop_back();
    this->p_data.pop_back();
    this->p2e.pop_back();
    this->p2p.pop_back();
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<ipair> AbstractPolygonMesh<M,V,E,P>::get_boundary_edges() const
{
    std::vector<ipair> res;
    for(uint eid=0; eid<this->num_edges(); ++eid)
    {
        if (this->edge_is_boundary(eid))
        {
            ipair e;
            e.first  = this->edge_vert_id(eid,0);
            e.second = this->edge_vert_id(eid,1);
            res.push_back(e);
        }
    }
    return res;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<uint> AbstractPolygonMesh<M,V,E,P>::get_boundary_vertices() const
{
    std::vector<uint> res;
    for(uint vid=0; vid<this->num_verts(); ++vid) if (this->vert_is_boundary(vid)) res.push_back(vid);
    return res;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
double AbstractPolygonMesh<M,V,E,P>::poly_mass(const uint pid) const
{
    return this->poly_area(pid);
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
double AbstractPolygonMesh<M,V,E,P>::poly_area(const uint pid) const
{
    double area = 0.0;
    std::vector<uint> tris = poly_tessellation(pid);
    for(uint i=0; i<tris.size()/3; ++i)
    {
        area += triangle_area(this->vert(tris.at(3*i+0)),
                              this->vert(tris.at(3*i+1)),
                              this->vert(tris.at(3*i+2)));
    }
    return area;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<int> AbstractPolygonMesh<M,V,E,P>::export_per_poly_labels() const
{
    std::vector<int> labels(this->num_polys());
    for(uint pid=0; pid<this->num_polys(); ++pid)
    {
        labels.at(this->poly_data(pid).label);
    }
    return labels;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
std::vector<Color> AbstractPolygonMesh<M,V,E,P>::export_per_poly_colors() const
{
    std::vector<Color> colors(this->num_polys());
    for(uint pid=0; pid<this->num_polys(); ++pid)
    {
        colors.at(pid) = this->poly_data(pid).color;
    }
    return colors;
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::poly_flip_winding_order(const uint pid)
{
    std::reverse(this->polys.at(pid).begin(), this->polys.at(pid).end());

    update_p_normal(pid);
    for(uint off=0; off<this->verts_per_poly(pid); ++off) update_v_normal(this->poly_vert_id(pid,off));
}

//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

template<class M, class V, class E, class P>
CINO_INLINE
void AbstractPolygonMesh<M,V,E,P>::operator+=(const AbstractPolygonMesh<M,V,E,P> & m)
{
    uint nv = this->num_verts();
    uint nf = this->num_polys();
    uint ne = this->num_edges();

    std::vector<uint> tmp;
    for(uint pid=0; pid<m.num_polys(); ++pid)
    {
        std::vector<uint> f;
        for(uint off=0; off<m.verts_per_poly(pid); ++off) f.push_back(nv + m.poly_vert_id(pid,off));
        this->polys.push_back(f);

        this->p_data.push_back(m.poly_data(pid));

        tmp.clear();
        for(uint eid : m.p2e.at(pid)) tmp.push_back(ne + eid);
        this->p2e.push_back(tmp);

        tmp.clear();
        for(uint nbr : m.p2p.at(pid)) tmp.push_back(nf + nbr);
        this->p2p.push_back(tmp);
    }
    for(uint eid=0; eid<m.num_edges(); ++eid)
    {
        this->edges.push_back(nv + m.edge_vert_id(eid,0));
        this->edges.push_back(nv + m.edge_vert_id(eid,1));

        this->e_data.push_back(m.edge_data(eid));

        tmp.clear();
        for(uint tid : m.e2p.at(eid)) tmp.push_back(nf + tid);
        this->e2p.push_back(tmp);
    }
    for(uint vid=0; vid<m.num_verts(); ++vid)
    {
        this->verts.push_back(m.vert(vid));
        this->v_data.push_back(m.vert_data(vid));

        tmp.clear();
        for(uint eid : m.v2e.at(vid)) tmp.push_back(ne + eid);
        this->v2e.push_back(tmp);

        tmp.clear();
        for(uint tid : m.v2p.at(vid)) tmp.push_back(nf + tid);
        this->v2p.push_back(tmp);

        tmp.clear();
        for(uint nbr : m.v2v.at(vid)) tmp.push_back(nv + nbr);
        this->v2v.push_back(tmp);
    }

    this->update_bbox();

    logger << "Appended " << m.mesh_data().filename << " to mesh " << this->mesh_data().filename << endl;
    logger << this->num_polys() << " faces" << endl;
    logger << this->num_verts() << " verts" << endl;
}

}
