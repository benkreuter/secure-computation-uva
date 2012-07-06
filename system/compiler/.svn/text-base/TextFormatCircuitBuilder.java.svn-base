import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Scanner;
import java.util.Vector;


public class TextFormatCircuitBuilder extends CircuitBuilder
{
    private File inputFile;
    private static int[] powersOf2 = { 1, 2, 4, 8, 16 };

    public TextFormatCircuitBuilder(String filename)
    {
        inputFile = new File(filename);
    }

    public Object[] buildCircuit() throws FileNotFoundException
    {
        Scanner input = new Scanner(inputFile);
        Vector ret = new Vector();

        while(input.hasNextLine())
            {
                String line = input.nextLine();
                if(line.length() > 0)
                    {  
                        String tokens[] = line.split(" ");
                        Gate newGate;
                        int id = Integer.parseInt(tokens[0]);
                        if(tokens[1].equals("input"))
                            {
                                newGate = new Gate(new ArrayList(), id, tokens[2]);
                            }
                        else if(tokens[1].equals("gate"))
                            {
                                ArrayList inputs = new ArrayList();
                                int arity = Integer.parseInt(tokens[3]);
                                boolean[] table = new boolean[powersOf2[arity]];
                                int currentIndex = 6;

                                for(int i = 0; i < powersOf2[arity]; i++)
                                    {
                                        int value = Integer.parseInt(tokens[currentIndex]);
                                        if (value == 0) {
                                            table[i] = false;
                                        } else if (value == 1) {
                                            table[i] = true;
                                        } else {
                                            System.err.println("Input File Error on gate: " + tokens[0]);
                                            System.exit(-1);
                                        }
                                        currentIndex++;
                                    }

                                currentIndex += 3;
                                newGate = new Gate(new ArrayList(), new ArrayList(), table, id);
                            
                                for (int i = 0; i < arity; i++) {
                                    int inputGateNum = Integer.parseInt(tokens[currentIndex]);
                                    inputs.add(ret.get(inputGateNum));

                                    ((Gate)ret.elementAt(inputGateNum)).getOutputs().add(newGate);

                                    currentIndex++;
                                }
                                newGate.setInputs(inputs);
                            }
                        else
                            {
                                int arity = Integer.parseInt(tokens[4]);
                                boolean[] table = new boolean[powersOf2[arity]];
                                ArrayList inputs = new ArrayList();
                                int currentIndex = 7;

                                for(int i = 0; i < powersOf2[arity]; i++)
                                    {
                                        int value = Integer.parseInt(tokens[currentIndex]);
                                        if (value == 0) {
                                            table[i] = false;
                                        } else if (value == 1) {
                                            table[i] = true;
                                        } else {
                                            System.err.println("Input File Error on gate: " + tokens[0]);
                                            System.exit(-1);
                                        }
                                        currentIndex++;
                                    }

                                currentIndex += 3;
                                newGate = new Gate(inputs, table, id);

                                for (int i = 0; i < arity; i++) {
                                    int inputGateNum = Integer.parseInt(tokens[currentIndex]);
                                    inputs.add(ret.get(inputGateNum));
                                    ((Gate)ret.elementAt(inputGateNum)).getOutputs().add(newGate);
                                    currentIndex++;
                                }
                                newGate.setInputs(inputs);
                                newGate.setComment(tokens[currentIndex+1]);
                            }

                        ret.add(newGate);
                    }
            }

        return ret.toArray();
    }
}