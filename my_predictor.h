
// my_predictor.h
// This file contains the implementation of the Virtual Program Counter Predictor 

#include <cmath>

class my_update : public branch_update {
public:
#define MAX_ITER	12
	unsigned int pred_iter;		//iteration value for the predicted target
	int cb_out[MAX_ITER];		//Output of perceptron for all virtual branches of an indirect branch
};

class my_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	59
#define TABLE_BITS	14
#define N	1021
	my_update u;				// object for my update for the VPC and Perceptron branch predictor
	branch_info bi;				// stores the branch info of the recently predicted branch
	unsigned long long history;		// Global History Resgister
	unsigned int targets[1<<TABLE_BITS];		//BTB containing target addresses
	unsigned char lfu_count[1<<TABLE_BITS];		// Frequently Used counter value for each entry in BTB
	char tab_perceptron[N][HISTORY_LENGTH+1];	// Table of perceptrons

	/* Hash function for producing a rondomized constant value for an iteration */
	unsigned int hash_func(unsigned int i){
		return ((unsigned int)((i*(unsigned long long)2654435761)&0xFFFFFFFF));
	}

	/* Initialize the variables of my predictor class*/
	my_predictor (void) : history(0) { 
		memset (targets, 0, sizeof (targets));
		memset (lfu_count, 0, sizeof (lfu_count));
		for(int i=0;i<N;i++)
		{
			memset (tab_perceptron[i], 0, sizeof(tab_perceptron[0]));
		}
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			//Do nothing
		}
		if (b.br_flags & BR_INDIRECT) {
			/* Initialize VPCA,VGHR,iter values */
			unsigned int vpca = b.address;
			unsigned int iter = 1;
			unsigned long long vghr = history;
			bool done = false;
			unsigned int pred_target;
			bool pred_direc;

			/* Set the default output value of perceptrons for all virtual branches to 0*/ 
			memset (u.cb_out, 0, sizeof (u.cb_out));
			
			while(!done)
			{
				pred_target = targets[vpca & ((1<<TABLE_BITS)-1)];
				pred_direc = cond_branch_predict(vpca,vghr,iter-1, u.cb_out);

				/* Check if there is no BTB miss and the predicted direction is taken */
				if(pred_target && (pred_direc==true))
				{
					/* Set the predicted target and stop further prediction */
					u.target_prediction(pred_target);
					done = true;
				}
				/* Check if there is a BTB miss or maximum iterations have been reached */
				else if(!pred_target || (iter >= MAX_ITER))
				{
					u.target_prediction(0);
					done = true;
				}
				else
				{
					/* Calculate VPCA and VGHR for next iteration */
					vpca = b.address^hash_func(iter);
					vghr = vghr<<1;
					iter++;
				}

			}

			u.pred_iter = iter;
		}
		return &u;
	}

	bool cond_branch_predict(unsigned int address, unsigned long long ghr, unsigned index, int * cb_out) {
		int i = address%N;	// Calculate the index in the perceptron table for the current address
		/* Add the bias w0 to the output */ 
		cb_out[index] += tab_perceptron[i][0];
		/* Calculate the complete perceptron output based on the summation of the dot product of the weights and input history*/
		for(int j=1;j<(HISTORY_LENGTH+1);j++)
		{
			if(((((unsigned long long)1)<<(j-1))&ghr)>0)
			{
				cb_out[index]+=tab_perceptron[i][j];
			}
			else
			{
				cb_out[index]-=tab_perceptron[i][j];
			}
		}
		
		/* Return the direction prediction based on whether the output is positive or negative */
		return (cb_out[index]>0);
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			// Do nothing
		}
		if (bi.br_flags & BR_INDIRECT) {
			/* Check if the target was correctly predicted */
			if((((my_update*)u)->target_prediction())==target)
			{
				unsigned int iter = 1;
				unsigned int vpca = bi.address;
				unsigned long long vghr = history;

				while(iter<(((my_update*)u)->pred_iter))
				{
					if(iter==(((my_update*)u)->pred_iter))
					{	
						/* Train the conditional branch predictor to predict the virtual branch as taken*/
						cond_branch_update(vpca,vghr,true,iter-1, (((my_update*)u)->cb_out));
						/* Increment the Frequently Used counter for this virtual branch */
						BTB_repl_update(vpca);
					}
					else
					{
						/* Train the conditional branch predictor to predict the virtual branch as Not taken*/
						cond_branch_update(vpca,vghr,false,iter-1, (((my_update*)u)->cb_out));
					}
					vpca = bi.address^hash_func(iter);
					vghr = vghr<<1;
					iter++;
				}

			}
			else
			{
				/* Initialize the VPCA,VGHR,iter values*/
				unsigned int iter = 1;
				unsigned int vpca = bi.address;
				unsigned int vpca_btb_miss = 0;		//vpca for the virtual branch that missed in the BTB
				unsigned int vpca_lru = 0;			//vpca for the virtual branch that is least frequently used
				unsigned int pred_target;
				unsigned long long vghr = history;
				bool target_found=false;
				unsigned char min=lfu_count[vpca & ((1<<TABLE_BITS)-1)];	//initialize the minimum value as the counter value for the first virtual branch
				unsigned int index;
				/* Loop through all the virtual branches until the correct target is found*/
				while((iter<=MAX_ITER) && (target_found==false))
				{
					index = vpca & ((1<<TABLE_BITS)-1);
					pred_target = targets[index];
					/* Check if the target for the current virtual branch matches with the correct target address*/ 
					if(pred_target==target)
					{	
						/* Check to see if the direction prediction is done at all for this virtual branch */
						if(iter<(((my_update*)u)->pred_iter))
						{
							cond_branch_update(vpca,vghr,true,iter-1, (((my_update*)u)->cb_out));
						}
						else
						{
							/* First compute the output of the perceptron for this virtual branch and then train the CBP to predict this branch as taken */
							cond_branch_predict(vpca,vghr,iter-1, (((my_update*)u)->cb_out));
							cond_branch_update(vpca,vghr,true,iter-1, (((my_update*)u)->cb_out));
						}
						BTB_repl_update(vpca);
						target_found = true;
					}
					else if(pred_target)
					{
						/* Check to see if the direction prediction is done at all for this virtual branch */
						if(iter<=(((my_update*)u)->pred_iter))
						{
							cond_branch_update(vpca,vghr,false,iter-1, (((my_update*)u)->cb_out));
						}
						else
						{
							/* First compute the output of the perceptron for this virtual branch and then train the CBP to predict this branch as Not taken */
							cond_branch_predict(vpca,vghr,iter-1, (((my_update*)u)->cb_out));
							cond_branch_update(vpca,vghr,false,iter-1, (((my_update*)u)->cb_out));
						}						
						/* Find the virtual branch that is least frequently used */
						if(lfu_count[index] < min)
						{
							min = lfu_count[index];
							vpca_lru = vpca;
						}
					}
					else
					{
						vpca_btb_miss = vpca;
						break;
					}

					vpca = bi.address^hash_func(iter);
					vghr = vghr<<1;
					iter++;
				}
				/* If the target was not found insert the correct target into the BTB */
				if(target_found==false)
				{
					unsigned int vpca_repl;
					/* If there was BTB miss insert the correct target into the corresponding virtual branch*/
					if(vpca_btb_miss != 0)
					{
						vpca_repl = vpca_btb_miss;
						targets[vpca_repl & ((1<<TABLE_BITS)-1)] = target;
						BTB_repl_update(vpca_repl);
					}
					else
					{
						/* Else insert the correct target in the BTB entry corresponding to the Least frequently used virtual branch */
						vpca_repl = vpca_lru;
						targets[vpca_repl & ((1<<TABLE_BITS)-1)] = target;
						memset (tab_perceptron[vpca_repl%N], 0, sizeof(tab_perceptron[0]));
						lfu_count[vpca_repl & ((1<<TABLE_BITS)-1)] = 1;
					}					
				}
			}
		}

		/* Update the Global History Register */
		history <<= 1;
		history |= taken;
		history &= (((unsigned long long)1)<<HISTORY_LENGTH)-1;
	}

	void cond_branch_update(unsigned int address,unsigned long long ghr,bool taken, unsigned char index, int *cb_out) {
		int i = address%N;
		bool pred_dir = (cb_out[index]>0);
		bool x;

		/* If the predicted outcome was wrong or the training threshold has not reached yet then train the perceptron */
		if((pred_dir!=taken) || (abs(cb_out[index])<127))
		{
			if(taken)
			{
				tab_perceptron[i][0]+=1;
			}
			else
			{
				tab_perceptron[i][0]-=1;
			}
			for(int j=1; j<(HISTORY_LENGTH+1);j++)
			{
				x= ((((unsigned long long)1)<<(j-1))&ghr)>0;
				if(taken == x)
				{
					tab_perceptron[i][j]+=1;
				}
				else
				{
					tab_perceptron[i][j]-=1;
				}
			}
			
		}
	}
	/* Increment the LFU Counter update */
	void BTB_repl_update(unsigned int address) {
		int index = address & ((1<<TABLE_BITS)-1);
		if(lfu_count[index] < 255)
		{
			lfu_count[index] += 1;
		}
	}
};
