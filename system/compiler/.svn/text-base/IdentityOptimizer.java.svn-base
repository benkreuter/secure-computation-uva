import java.util.Vector;

public class IdentityOptimizer extends Optimizer
{
    public Object[] optimize(Object[] circuit)
    {
        // Remove all identity gates
        Vector ret = new Vector(circuit.length);
        for(int i = 0; i < circuit.length; i++)
            {
                Gate cur = (Gate)circuit[i];
                if(cur == null)
                    {
                    }
                else if(cur.isInput())
                    {
                        ret.add(cur);
                    }
                else if(cur.isOutput())
                    {
                        ret.add(cur);
                    }
                else
                    {
                        if(cur.getTruthTable().length > 2)
                            {
                                ret.add(cur);
                            }
                        else
                            {
                                //                                System.err.println(cur.getTruthTable());
                                // Possibly an identity gate
                                if((cur.getTruthTable()[0] == false) && (cur.getTruthTable()[1] == true))
                                    {
                                        // Remove this identity gate
                                        for(int j = 0; j < cur.getOutputs().size(); j++)
                                            {
                                                Gate g = (Gate)cur.getOutputs().get(j);
                                                for(int k = 0; k < g.getInputs().size(); k++)
                                                    {
                                                        if(((Gate)(g.getInputs().get(k))).id == cur.id)
                                                            {
                                                                g.getInputs().set(k,cur.getInputs().get(0));
                                                            }
                                                    }
                                                ((Gate)cur.getInputs().get(0)).getOutputs().add(cur.getOutputs().get(j));
                                            }
                                        // update outputs
                                        for(int j = 0; j < ((Gate)cur.getInputs().get(0)).getOutputs().size(); j++)
                                            {
                                                if(((Gate)((Gate)cur.getInputs().get(0)).getOutputs().get(j)).id == cur.id)
                                                    ((Gate)cur.getInputs().get(0)).getOutputs().remove(j);
                                            }
                                        circuit[i] = null;
                                    }
                                else if((cur.getTruthTable()[0] == true) && (cur.getTruthTable()[1] == false))
                                    {
                                        // Remove this inverter
                                        for(int j = 0; j < cur.getOutputs().size(); j++)
                                            {
                                                Gate g = (Gate)cur.getOutputs().get(j);
                                                // Rewrite the truth table
                                                int idx = -1;
                                                for(int k = 0; k < g.getInputs().size(); k++)
                                                    {
                                                        if(((Gate)g.getInputs().get(k)).id == cur.id)
                                                            idx = k;
                                                    }

                                                boolean table[] = new boolean[g.getTruthTable().length];
                                                for(int k = 0; k < g.getTruthTable().length; k++)
                                                    {
                                                        switch(idx)
                                                            {
                                                            case 0:
                                                                if(k % 2 == 0) table[k] = g.getTruthTable()[k+1];
                                                                else table[k] = g.getTruthTable()[k-1];
                                                                break;
                                                            case 1:
                                                                if((k/2) % 2 == 0) table[k] = g.getTruthTable()[k+2];
                                                                else table[k] = g.getTruthTable()[k-2];
                                                                break;
                                                            case 2:
                                                                if((k/4) % 2 == 0) table[k] = g.getTruthTable()[k+4];
                                                                else table[k] = g.getTruthTable()[k-4];
                                                                break;
                                                            default:
                                                                break;
                                                            }
                                                    }
                                                g.setTruthTable(table);
                                                g.getInputs().set(idx,cur.getInputs().get(0));
                                                ((Gate)cur.getInputs().get(0)).getOutputs().add(cur.getOutputs().get(j));
                                            }
                                        /*
                                         * All wrong!  Don't use
                                         * indexes like this!  Should
                                         * use pointers to gate
                                         * objects!
                                         *
                                         * Need to fix this in the
                                         * builder class, and then
                                         * everywhere else.
                                         */
                                        for(int j = 0; j < ((Gate)cur.getInputs().get(0)).getOutputs().size(); j++)
                                            {
                                                if(((Gate)((Gate)cur.getInputs().get(0)).getOutputs().get(j)).id == cur.id)
                                                    ((Gate)cur.getInputs().get(0)).getOutputs().remove(j);
                                            }
                                        circuit[i] = null;
                                    }
                                else
                                    {
                                        ret.add(cur);
                                    }
                            }
                    }
            }
        return ret.toArray();
    }
}