/* A graph reachability checker */

#include <stdio.h>
#include "misc.h"
#undef GLOBAL
#define GLOBAL
#include "prune.h"

#define MAX_NODES	256
#define MAX_WIDTH	((MAX_NODES+31)/32)

#define CanReach(i,j)	(ReachMat[i][j/32] & (1l << (j%32)))

unsigned long ReachMat[MAX_NODES][MAX_WIDTH] = {{0l}};

short topnode = 0, linkTop = 0, comparedepth = 10;

/*
 * The prototype reads the graph in from a file containing lines of the
 * form:
 *	source_node dest_node [ dest_node ... ] 0
 *
 */

#ifdef DEBUG

void 
ReadGraph(void)
{
    short n, d;
    FILE *fp = fopen("graph", "rt");
    for (n = 0; n < MAX_NODES; n++)
    {
	for (d = 0; d < MAX_WIDTH; d++)
	{
	    ReachMat[n][d] = 0l;
	}
    }
    while (fscanf(fp, "%d", &n) == 1 && !feof(fp) && n > 0)
    {
	if (n > topnode)
	    topnode = n;
	while (fscanf(fp, "%d", &d) == 1 && !feof(fp) && d > 0)
	{
	    ReachMat[n][d / 32] |= (1l << (d % 32));
	}
    }
}
#else
void 
ReadGraph(void)
{
    short n, d;
    readNodes();
    for (n = 0; n < MAX_NODES; n++)
    {
	for (d = 0; d < MAX_WIDTH; d++)
	{
	    ReachMat[n][d] = 0l;
	}
    }
    for (n = 1; n < linkTop; n++)
    {
	int i;
	if (treeTable[n].refcnt == 0)
	    continue;
	for (i = 0; i < treeTable[n].nchild; i++)
	{
	    d = treeTable[n].child[i];
	    ReachMat[n][d / 32] |= (1l << (d % 32));
	}
    }
    topnode = linkTop - 1;
    dumpTree(stdout, 1);
}
#endif

void 
FindReach(void)
{
    int i, j, k, changed = 1;
    while (changed)
    {
	changed = 0;
	for (i = 1; i <= topnode; i++)
	{
	    for (j = 1; j <= topnode; j++)
	    {
		if (CanReach(i, j))
		{
		    for (k = 0; k <= ((topnode + 31) / 32); k++)
		    {
			unsigned long v;
			v = ReachMat[i][k] | ReachMat[j][k];
			if (v != ReachMat[i][k])
			{
			    changed = 1;
			    ReachMat[i][k] = v;
			}
		    } //for k
			} //can reach
			} //for j
			    } //for i
				} //changed
				}

				void PrintGraph(void)
				{
				    int i, j;
				        printf("\n\nNODE REACHABILITY ANALYSIS RESULTS\n");
				        printf("==== ============ ======== =======\n\n");
				    for (i = 1; i <= topnode; i++)
				    {
					printf("%d ", i);
					for (j = 1; j <= topnode; j++)
					{
					    if (CanReach(i, j))
						printf("%d ", j);
					}
					    printf("\n");
				    }
				}

		    short classes[MAX_NODES], transient[MAX_NODES];

		    void AnalyzeIt(void)
		    {
			int i, j, k, same, disjoint;
			for (i = 1; i <= topnode; i++)
			{
			    classes[i] = i;
			    transient[i] = 0;
			}
			/*
			 * We now loop through all the classes, comparing
			 * them. If any two are identical, we set the higher
			 * to be the lower. In the end we will have a small
			 * number of classes, representing the different
			 * subgraphs. If there is only one, the graph is
			 * strongly connected. Otherwise, the graph (having a
			 * single entry point), consists of some transient
			 * states, and one or more disjoint strongly
			 * connected subgraphs. The transients are any
			 * classes that are supersets, containing one or more
			 * of the subgraphs plus some extra nodes (it is the
			 * extra nodes that are the actual transients).
			 */
			for (i = 1; i < topnode; i++)
			{
			    for (j = i + 1; j <= topnode; j++)
			    {
				same = disjoint = 1;
				if (classes[j] != j)
				    continue;
				for (k = 0; k <= (topnode + 31) / 32; k++)
				{
				    if (ReachMat[i][k] & ReachMat[j][k])
					disjoint = 0;
				    if (ReachMat[i][k] != ReachMat[j][k])
				    {
					same = 0;
					break;
				    }
				}
				if (same)
				    classes[j] = classes[i];
				if (!same && !disjoint)
				{
				    for (k = 1; k <= topnode; k++)
				    {
					if (CanReach(i, k) && !CanReach(j, k))
					{
					    transient[k] = 1;
					    classes[i] = j;
					} else if (CanReach(j, k) && !CanReach(i, k))
					{
					    transient[k] = 1;
					    classes[j] = i;
					}
				    }
				}
			    }
			}
			printf("\n\nSTRONGLY-CONNECTED SUBGRAPHS\n================== =========\n\n");
			for (j = i = 1; i <= topnode; i++)
			{
			    if (classes[i] == i)
			    {
				printf("Class %d Nodes ", j++);
				for (k = 1; k <= topnode; k++)
				    if (CanReach(i, k))
					printf("%d ", k);
				printf("\n");
			    }
			}
			/*
			 * Check for any unreachable ones; should only be
			 * (possibly) 1
			 */
			for (i = 2; i <= topnode; i++)
			    for (j = 0; j <= (topnode + 31) / 32; j++)
				ReachMat[1][j] |= ReachMat[i][j];
			for (i = 1; i <= topnode; i++)
			    if (!CanReach(1, i))
				transient[i] = 1;
			printf("\n\nTRANSIENT NODES\n========= =====\n\n");
			for (i = 1; i <= topnode; i++)
			    if (transient[i])
				printf("%d ", i);
			printf("\n");
		    }

		    char *USAGE[] = {
			"This program is a reachability checker for Behaviour Graphs. It\n",
			"reads the graph from the normal files (TREE.BIN, etc), and produces\n",
			"a reachability analysis as output. A `healthy' specification should\n",
			"have at most about three transient nodes, and apart from these, only\n",
			"one reachability class. If this is not the case, the specification is\n",
			"at best poorly written, while at worst it suffers from livelock or\n",
			"deadlock.\n\nNote that at this stage only the BG nodes are analysed;\n",
			"each of these nodes in turn contains a sequence of zero or more transition\n",
			"executions. Ideally the analysis would extend to these, in which case\n",
			"we could actually automatically determine exactly what transitions are\n",
			"transient, etc. At present, the reachability checker merely indicates\n",
			"whether there is a problem or not, and where in the BG to start looking.\n",
			NULL
		    };

		    void main(int argc, char *argv[])
		    {
			if (argc != 1)
			{
			    int i = 0;
			    while (USAGE[i])
				    fprintf(stderr, USAGE[i++]);
			        exit(0);
			        (void)argv;
			}
			ReadGraph();
			FindReach();
			PrintGraph();
			AnalyzeIt();
		    }
