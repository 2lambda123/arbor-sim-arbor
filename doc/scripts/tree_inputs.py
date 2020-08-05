import math

npos = -1

def add_vec(u, v):
    return (u[0]+v[0], u[1]+v[1])

def norm_vec(v):
    return math.sqrt(v[0]*v[0] + v[1]*v[1])

def sub_vec(u, v):
    return (u[0]-v[0], u[1]-v[1])

def rot90_vec(v):
    return (v[1], -v[0])

def scal_vec(alpha, v):
    return (alpha*v[0], alpha*v[1])

def unit_vec(v):
    L = norm_vec(v)
    return (v[0]/L, v[1]/L)

def is_collocated(x, y):
    return x[0]==y[0] and x[1]==y[1]

class Segment:
    def __init__(self, prox, dist, tag):
        self.prox = prox
        self.dist = dist
        self.tag = tag
        self.length = norm_vec(sub_vec(prox, dist))

    def location(self, pos):
        b = self.prox
        e = self.dist
        return (b[0]+pos*(e[0]-b[0]), b[1]+pos*(e[1]-b[1]), b[2]+pos*(e[2]-b[2]))

    def corners(self, prox=0, dist=1):
        b = self.location(prox)
        e = self.location(dist)
        rb = b[2]
        re = e[2]
        d = sub_vec(self.dist, self.prox)
        o = rot90_vec(unit_vec(d))
        p1 = add_vec(b, scal_vec(rb, o))
        p2 = add_vec(e, scal_vec(re, o))
        p3 = sub_vec(e, scal_vec(re, o))
        p4 = sub_vec(b, scal_vec(rb, o))
        return [p1,p2,p3,p4]

    def __str__(self):
        return 'seg({}, {})'.format(self.prox, self.dist)

    def __repr__(self):
        return 'seg({}, {})'.format(self.prox, self.dist)


# Represent and query a cable cell branch for rendering.
# The branch is composed of segments, which are grouped into "sections".
# A section is a fully connected sequence of segments, and a branch
# will have more than one section if there are jumps/gaps between
# segments.
# The section representation makes things a bit messy, but it is required
# to be able to draw morphologies with gaps.

class Branch:
    def __init__(self, tree, indexes):
        X = tree['x']
        Y = tree['y']
        R = tree['r']
        T = tree['t']

        L = 0
        self.sections = []
        for sec in indexes:
            segs = []
            for i in range(len(sec)-1):
                pid = sec[i]
                did = sec[i+1]
                prox = (X[pid], Y[pid], R[pid])
                dist = (X[did], Y[did], R[did])
                if not is_collocated(prox, dist):
                    segs.append(Segment(prox, dist, T[did]))
                    L += segs[-1].length
            self.sections.append(segs)
        self.length = L

    def minmax(self):
        minx = math.inf
        miny = math.inf
        maxx = -math.inf
        maxy = -math.inf
        for sec in self.sections:
            for seg in sec:
                px, py, pr = seg.prox
                dx, dy, dr = seg.dist
                minx = min(minx, px-pr, dx-dr)
                miny = min(miny, py-pr, dy-dr)
                maxx = max(maxx, px+pr, dx+dr)
                maxy = max(maxy, py+pr, dy+dr)

        return (minx, maxx, miny, maxy)

    # Return the segment location that contains the location
    # that is 0 ≤ pos ≤ 1 along the branch
    #
    # return tuple: (sec, seg, pos)
    #       sec: id of the section containing the segment
    #       seg: index of the segment in the section
    #       pos: relative position of the location inside the segment
    def segment_id(self, pos):
        assert(pos>=0 and pos<=1)
        if pos==0:
            return (0, 0, 0.0)
        if pos==1:
            return (len(self.sections)-1, len(self.sections[-1])-1, 1.0)
        l = pos * self.length

        part = 0
        for secid in range(len(self.sections)):
            sec = self.sections[secid]
            for segid in range(len(sec)):
                seg = sec[segid]
                if part+seg.length >= l:
                    segpos = (l-part)/seg.length
                    return (secid, segid, segpos)

                part += seg.length

    def location(self, pos):
        assert(pos>=0 and pos<=1)

        secid, segid, segpos = self.segment_id(pos)
        return self.sections[secid][segid].location(segpos)

    def sec_outline(self, secid, pseg, ppos, dseg, dpos):
        sec = self.sections[secid]

        # Handle the case where the cable is in one segment
        if pseg==dseg:
            assert(ppos<=dpos)
            return sec[pseg].corners(ppos, dpos)

        left = []
        right = []

        # Handle the partial partial proximal segment
        p1, p2, p3, p4 = sec[pseg].corners(ppos, 1)
        left += [p1, p2]
        right += [p4, p3]

        # Handle the full segments in the middle
        for segid in range(pseg+1, dseg):
            p1, p2, p3, p4 = sec[segid].corners()
            left += [p1, p2]
            right += [p4, p3]

        # Handle the partial distal segment
        p1, p2, p3, p4 = sec[dseg].corners(0, dpos)
        left += [p1, p2]
        right += [p4, p3]

        right.reverse()
        return left + right



    # Return outline of all (sub)sections in the branch between the relative
    # locations: 0 ≤ prox ≤ dist ≤ 1
    def outline(self, prox=0, dist=1):
        psec, pseg, ppos = self.segment_id(prox)
        dsec, dseg, dpos = self.segment_id(dist)

        if psec==dsec and pseg==dseg:
            return [self.sections[psec][pseg].corners(ppos, dpos)]
        if psec==dsec:
            return [self.sec_outline(psec, pseg, ppos, dseg, dpos)]

        outlines = [self.sec_outline(psec, pseg, ppos, len(self.sections[psec])-1, 1)]
        for secid in range(psec+1,dsec):
            outlines.append(self.sec_outline(secid, 0, 0, len(self.sections[secid])-1, 1))
        outlines.append(self.sec_outline(dsec, 0, 0, dseg, dpos))

        return outlines

# A morphology for rendering is a flat list of branches, with no
# parent-child information for the branches.
# Each branch is itself a list of sections, where each section
# represents a sequence of segments with no gaps.

def make_morph(tree, branches):
    m = []
    for branch_sections in branches:
        m.append(Branch(tree, branch_sections))

    return m

# single cable segment
tree2a = {
    'p': [npos, 0],
    'x': [0, 10],
    'y': [0, 0],
    'r': [1, 0.5],
    't': [3, 3],
}
branches2a = [[[0, 1]]]

# unbranched cable with 4 segments
tree2b = {
    'p': [npos, 0, 1, 2, 3],
    'x': [0, 3, 5, 8, 10],
    'y': [0, 0.2, -0.1, 0,   0],
    'r': [1, 0.8, 0.7,  0.6, 0.5],
    't': [1,  1,  2,  2,  3],
}
branches2b = [[[0, 1, 2, 3, 4]]]

# unbranched cable with discontinuity of radius.
tree2c = {
    'p': [npos, 0, 1, 2, 3, 4],
    'x': [0, 3, 5, 8, 8, 10],
    'y': [0, 0.2, -0.1, 0, 0,  0],
    'r': [1, 0.8, 0.7,  0.6, 0.3, 0.5],
    't': [1,  1,  2,  2,  3, 3],
}
branches2c = [[[0, 1, 2, 3, 4, 5]]]

# Y shaped cells
tree3a = {
    'p': [npos, 0, 1, 1],
    'x': [0, 10, 15, 15],
    'y': [0, 0, 3, -3],
    'r': [1, 0.5, 0.25, 0.25],
    't': [3,  3,  3,  3],
}
branches3a = [[[0, 1]], [[1, 2]], [[1, 3]]]

tree3b = {
    'p': [npos, 0, 1,  2,  1,  4,],
    'x': [0, 10,  10, 15, 10, 15,],
    'y': [0, 0,    0,  3,  0, -3,],
    'r': [1, 0.5,  0.25, 0.25,  0.25,  0.25],
    't': [3,  3,  3,  3,  3,  3],
}
branches3b = [[[0, 1]], [[1, 2, 3]], [[1, 4, 5]]]

tree3c = {
    'p': [npos, 0,      1,    2,  2],
    'x': [0,   10,     10,   15, 15],
    'y': [0,    0,      0,    3, -3],
    'r': [1,    0.5, 0.25, 0.25, 0.25],
    't': [3,    3, 3, 3, 3],
}
branches3c = [[[0, 1, 2]], [[2, 3]], [[2, 4]]]

# ball and stick
tree4 = {
    'p': [npos, 0, 1],
    'x': [0, 2, 10],
    'y': [0, 0, 0],
    'r': [2, 1, 1],
    't': [1, 1, 3]
}
branches4a = [[[0, 1, 2]]]

# soma + Y shaped dendrite
tree5 = {
   'x': [ 0,  3,     5,  10  , 15, 18,   23,   20],
   'y': [ 0,  -.8,    -1,   0.5,  0,  5,    8,   -4],
   'r': [ 3,  0.8, 0.8,  0.5,  0.5,  0.3,  0.3,  0.3],
   'p': [-1,  0,     1,   2  ,  3,    4,  5,    4],
   't': [ 1,  1,     3,   3  ,  3,    2,   2,   3]
}
branches5 = [[[0, 1, 2, 3]], [[3, 4, 5]], [[3, 6]]]

tree6 = {
   'x': [ 0, 6,   6, 15.0, 20.0, 21.0,  -5.0],
   'y': [ 0, 0,   0,  5.0,  7.0, -3.0,   0.0],
   'r': [ 2, 2, 0.5,  0.5,  0.3,  0.3,   0.5],
   'p': [-1, 0,   1,    2,    3,    2,     0],
   't': [ 1, 1,   3,    3,    3,    3,     2]
}
branches6 = [[[0, 1]], [[2, 3, 4]], [[2, 5]], [[0, 6]]]

#morph2a = make_morph(tree2a, branches2a)
#morph2b = make_morph(tree2b, branches2b)
#morph2c = make_morph(tree2c, branches2c)
#morph3a = make_morph(tree3a, branches3a)
#morph3b = make_morph(tree3b, branches3b)
#morph3c = make_morph(tree3c, branches3c)
#morph4  = make_morph(tree4,  branches4a)
#morph5  = make_morph(tree5, branches5)
#morph6  = make_morph(tree6, branches6)

# The tree used for the region & locset examples
treelab = {
   'x': [ 0, 4,   4,  8.0, 12.0, 20.0,  26.0, 12.0, 19.0, 24.0, 23.0, 26.0, -7.0, -10],
   'y': [ 0, 0,   0,    0, -0.5,  4.0,   6.0, -0.5, -3.0, -7.0, -1.0, -2.0,  0.0,   0.0],
   'r': [ 2, 2,0.75, 0.75, 0.75,  0.4,   0.2,  0.5,  0.5,  0.2,  0.2,  0.2,  0.4,   0.4],
   'p': [-1, 0,   1,    2,    3,    4,     5,    4,    7,    8,    8,   10,    0,    12],
   't': [ 1, 1,   3,    3,    3,    3,     3,    3,    3,    3,    3,    3,    2,     2]
}
brancheslab = [[[0,1,2,3,4]], [[4,5,6]], [[4,7,8]], [[8,9]], [[8,10,11]], [[0,12,13]]]

morphlab = make_morph(treelab, brancheslab)
