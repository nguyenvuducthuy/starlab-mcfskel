#pragma once
#include "SurfaceMeshHelper.h"
//#include "LegacyLaplacianHelper.h"
#include "LaplacianHelper.h"
#include "MatlabSurfaceMeshHelper.h"

/// Augments the setContraints with poles information
class PoleAttractorHelper : public MeanValueLaplacianHelper, public MatlabSurfaceMeshHelper{
public:
    PoleAttractorHelper(SurfaceMeshModel* mesh) : 
        SurfaceMeshHelper(mesh), 
        LaplacianHelper(mesh), 
        MeanValueLaplacianHelper(mesh), 
        MatlabSurfaceMeshHelper(mesh){}
    
    void createLaplacianMatrix(){
        /// Fill memory
        Size nv = mesh->n_vertices();
        Counter nzmax = mesh->n_halfedges()+mesh->n_vertices();        
        mxArray* _L = mxCreateSparse(nv, nv, nzmax, mxREAL);
        double*  sr  = mxGetPr(_L);
        mwIndex* irs = mxGetIr(_L);
        mwIndex* jcs = mxGetJc(_L);
        
        /// Fill sparse matrix in order
        int k=0; /// Tot number elements
        int j=0; /// Column index
        typedef std::pair<Index,Scalar> Key;        
        foreach(Vertex v, mesh->vertices()){
            jcs[j] = k;

            /// Sort off elements (including diagonal)
            QVector<Key> indices;

            /// If cotangent then it's symmetric and we can fill
            /// in the diagonal elements directly...
            Scalar diagel=0;
            foreach(Halfedge h, mesh->onering_hedges(v))
                diagel+=hweight[h];
            indices.push_back(Key(vindex[v],-diagel));
            
            /// Only off-diagonal elements are inserted!!
            foreach(Halfedge h, mesh->onering_hedges(v)){
                Index vi = vindex[mesh->to_vertex(h)];
                indices.push_back(Key(vi,hweight[h]));
            }
            
            /// Sort them in order
            std::sort(indices.begin(), indices.end());
            
            /// Insert them in the sparse matrix
            foreach(Key key,indices){
                // qDebug() << key.first << key.second;
                sr[k] = key.second;
                irs[k] = key.first;
                k++;
            }
            j++;
        }
        jcs[mesh->n_vertices()] = k;      
        put("L", _L);
    }
    
    void setConstraints(ScalarVertexProperty omega_H, ScalarVertexProperty omega_L){
        if(!omega_H || !omega_L) throw MissingPropertyException("Invalid");
        if(!vindex)  throw MissingPropertyException("v:index");
        
        /// Initialize "omega_L"
        {
            mxArray* _w = mxCreateDoubleMatrix(mesh->n_vertices(),1,mxREAL);
            double* w = mxGetPr(_w); 
            foreach(Vertex v, mesh->vertices())
                w[ vindex[v] ] = omega_L[v];
            put("omega_L", _w);
        }
        
        /// Initialize "omega_H"
        {
            mxArray* _w = mxCreateDoubleMatrix(mesh->n_vertices(),1,mxREAL);
            double* w = mxGetPr(_w); 
            foreach(Vertex v, mesh->vertices())
                w[ vindex[v] ] = omega_H[v];
            put("omega_H", _w);
        }
        
        /// Initialize x0
        {
            const char* x0_property = "v:point";
            Vector3VertexProperty points = mesh->get_vertex_property<Vector3>(x0_property);        
            if(!points) throw MissingPropertyException(x0_property);
            mxArray* _x0 = mxCreateDoubleMatrix(mesh->n_vertices(),3,mxREAL);
            double* x0 = mxGetPr(_x0);
            Index nrows = mesh->n_vertices();
            foreach(Vertex v, mesh->vertices()){
                // qDebug() << points[v];
                x0[ vindex[v] + 0*nrows ] = points[v].x();
                x0[ vindex[v] + 1*nrows ] = points[v].y();
                x0[ vindex[v] + 2*nrows ] = points[v].z();
            }
            put("x0", _x0);
        }
    }
    
    void setConstraints(ScalarVertexProperty omega_H, ScalarVertexProperty omega_L, ScalarVertexProperty omega_P, Vector3VertexProperty poles){
        /// Do what was already there
        setConstraints(omega_H,omega_L);
        if(!omega_P) throw MissingPropertyException("Invalid");
        BoolVertexProperty vissplit = mesh->get_vertex_property<bool>("v:issplit");
        
        /// Initialize omega_P (poles constraints)
        {
            mxArray* _w = mxCreateDoubleMatrix(mesh->n_vertices(),1,mxREAL);
            double* w = mxGetPr(_w); 
            foreach(Vertex v, mesh->vertices())
                w[ vindex[v] ] = omega_P[v];
            put("omega_P", _w);            
        }
        
        /// Initialize p0 (poles positions)
        {
            mxArray* _p0 = mxCreateDoubleMatrix(mesh->n_vertices(),3,mxREAL);
            double* p0 = mxGetPr(_p0);
            Index nrows = mesh->n_vertices();
            foreach(Vertex v, mesh->vertices()){
                if(vissplit[v]) continue;
                p0[ vindex[v] + 0*nrows ] = poles[v].x();
                p0[ vindex[v] + 1*nrows ] = poles[v].y();
                p0[ vindex[v] + 2*nrows ] = poles[v].z();
            }
            put("p0", _p0);
        }
    }
    void solve(){
        // eval("save('/Users/ata2/Developer/skelcollapse/poles.mat')");        
        eval("lastwarn('');");       
        eval("nv = size(L,1);");
        eval("OMEGA_L = spdiags(omega_L,0,nv,nv);");
        eval("L = OMEGA_L * L';");
        eval("H = spdiags(omega_H, 0, nv, nv);");
        eval("P = spdiags(omega_P, 0, nv, nv);");
        eval("LHS = [L; H; P];");
        eval("RHS = [ zeros(nv,3) ; H*x0 ; P*p0];");
        eval("x = LHS \\ RHS;");
        check_for_warnings();
    }
    
    void extractSolution(const string property){
        Vector3VertexProperty solution = mesh->get_vertex_property<Vector3>(property); 
        mxArray* _x = get("x");
        if(_x == NULL) throw StarlabException("matlab solver failure");
        double* x = mxGetPr(_x);
        Index nrows = mesh->n_vertices();
        foreach(Vertex v, mesh->vertices()){
            solution[v].x() = x[vindex[v] + nrows*0];
            solution[v].y() = x[vindex[v] + nrows*1];
            solution[v].z() = x[vindex[v] + nrows*2];
        }
    }
}; 
