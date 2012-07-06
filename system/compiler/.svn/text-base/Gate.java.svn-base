import java.util.List;
import java.util.ArrayList;
import java.util.HashSet;

public class Gate
{
    private ArrayList inputs, outputs;
    boolean[] truthTable;
    boolean isOutput, isInput;
    String comment;
    public int id;

    public Gate(List i, int id, String s)
    {
        isInput = true;
        isOutput = false;
        outputs = new ArrayList(i);
        comment = s;
        this.id = id;
    }

    public Gate(List o, boolean[] t, int id)
    {
        isOutput = true;
        isInput = false;
        inputs = new ArrayList(o);
        truthTable = t.clone();
        this.id = id;
    }

    public Gate(List i, List o, boolean[] t, int id)
    {
        inputs = new ArrayList(i);
        outputs = new ArrayList(o);
        truthTable = t.clone();
        isOutput = isInput = false;
        this.id = id;
    }

    public Gate(Gate g)
    {
        inputs = new ArrayList(g.inputs);
        outputs = new ArrayList(g.outputs);
        truthTable = g.truthTable.clone();
        isOutput = g.isOutput;
        isInput = g.isInput;
        id = g.id;
    }

    public String getComment()
    {
        return comment;
    }

    public void setComment(String s)
    {
        comment = s;
    }

    public Object clone()
    {
        return new Gate(this);
    }

    public List getInputs()
    {
        return inputs;
    }

    public List getOutputs()
    {
        return outputs;
    }

    public boolean[] getTruthTable()
    {
        return truthTable;
    }

    public void setTruthTable(boolean[] b)
    {
        truthTable = b.clone();
    }

    public void setInputs(ArrayList g)
    {
        inputs = g;
    }

    public boolean isInput()
    {
        return isInput;
    }

    public boolean isOutput()
    {
        return isOutput;
    }
}

// public class Gate {
// 	static int[] powersOf2 = { 1, 2, 4, 8, 16 };

// 	int id, numInputs, tableSize;
// 	boolean isInput, isOutput, leadsToOutput, isIdentity, isInverse,
// 			isContradiction, isTautology, isRemovable;
// 	boolean[] truthTable;

// 	Gate[] inputGates;
// 	int[] inputGateIndexes;

// 	HashSet<Gate> outputGates;
// 	String inputName, outputName;

// 	public Gate() {
// 		this.id = 0;
// 		this.numInputs = 0;
// 		this.tableSize = 0;
// 		this.inputGates = null;
// 		this.inputGateIndexes = null;
// 		this.truthTable = null;
// 		this.isInput = false;
// 		this.isOutput = false;
// 		this.leadsToOutput = false;
// 		this.isIdentity = false;
// 		this.isInverse = false;
// 		this.isContradiction = false;
// 		this.isTautology = false;
// 		this.isRemovable = false;
// 		this.outputGates = new HashSet<Gate>();
// 	}

// 	public Gate(int numInputs, Gate[] inputGates, boolean[] truthTable) {
// 		this();
// 		this.numInputs = numInputs;
// 		this.inputGates = inputGates;
// 		this.truthTable = truthTable;
// 	}

// 	public Gate(String gateDescription) {
// 		this();
// 		String[] tokens = gateDescription.split(" ");
// 		this.id = Integer.parseInt(tokens[0]);
// 		/**
// 		 * Format: 0 input //output$input.alice$0
// 		 */
// 		if (tokens[1].equals("input")) {
// 			numInputs = 1;
// 			this.inputName = tokens[2];
// 			this.isInput = true;
// 		}
// 		/**
// 		 * Format: 8 gate arity 2 table [ 0 0 0 1 ] inputs [ 0 0 ]
// 		 */
// 		else if (tokens[1].equals("gate")) {
// 			this.numInputs = Integer.parseInt(tokens[3]);
// 			this.tableSize = powersOf2[numInputs];
// 			this.inputGates = new Gate[numInputs];
// 			this.inputGateIndexes = new int[numInputs];
// 			this.truthTable = new boolean[tableSize];
// 			int currentIndex = 6;
// 			// Parse truth table
// 			for (int i = 0; i < tableSize; i++) {
// 				int value = Integer.parseInt(tokens[currentIndex]);
// 				if (value == 0) {
// 					truthTable[i] = false;
// 				} else if (value == 1) {
// 					truthTable[i] = true;
// 				} else {
// 					System.err.println("Input File Error on gate: " + this.id);
// 					System.exit(0);
// 				}
// 				currentIndex++;
// 			}
// 			// Parse input gates
// 			currentIndex += 3;
// 			for (int i = 0; i < numInputs; i++) {
// 				int inputGateNum = Integer.parseInt(tokens[currentIndex]);
// 				inputGateIndexes[i] = inputGateNum;
// 				currentIndex++;
// 			}
// 		}
// 		/**
// 		 * Format: 111 output gate arity 1 table [ 0 1 ] inputs [ 16 ]
// 		 * //output$output.alice$0
// 		 */
// 		else if (tokens[1].equals("output")) {
// 			this.numInputs = Integer.parseInt(tokens[4]);
// 			this.tableSize = powersOf2[numInputs];
// 			this.inputGates = new Gate[numInputs];
// 			this.inputGateIndexes = new int[numInputs];
// 			this.truthTable = new boolean[tableSize];
// 			this.isOutput = true;
// 			int currentIndex = 7;
// 			// Parse truth table
// 			for (int i = 0; i < tableSize; i++) {
// 				int value = Integer.parseInt(tokens[currentIndex]);
// 				if (value == 0) {
// 					truthTable[i] = false;
// 				} else if (value == 1) {
// 					truthTable[i] = true;
// 				} else {
// 					System.err.println("Input File Error on gate: " + this.id);
// 					System.exit(0);
// 				}
// 				currentIndex++;
// 			}
// 			// Parse input gates
// 			currentIndex += 3;
// 			for (int i = 0; i < numInputs; i++) {
// 				int inputGateNum = Integer.parseInt(tokens[currentIndex]);
// 				inputGateIndexes[i] = inputGateNum;
// 				currentIndex++;
// 			}
// 			// Parse output name
// 			currentIndex += 1;
// 			outputName = tokens[currentIndex];
// 		} else {
// 			System.err.println("Input File Error on gate: " + this.id);
// 			System.exit(0);
// 		}

// 		updateBooleanValues();
// 	}

// 	public boolean updateBooleanValues() {
// 		if (this.isInput || this.isOutput || numInputs > 1)
// 			return false;
// 		if (!truthTable[0] && truthTable[1]) {
// 			this.isIdentity = true;
// 			return true;
// 		} else if (truthTable[0] && !truthTable[1]) {
// 			this.isInverse = true;
// 			return true;
// 		}

// 		this.isTautology = true;
// 		this.isContradiction = true;
// 		for (int i = 0; i < tableSize; i++) {
// 			this.isTautology &= truthTable[i];
// 			this.isContradiction &= !truthTable[i];
// 		}
// 		if (this.isTautology || this.isContradiction) {
// 			return true;
// 		}
// 		return false;
// 	}

// 	public void addOutput(Gate output) {
// 		outputGates.add(output);
// 	}

// 	public void setInput(String input) {
// 		isInput = true;
// 		this.inputName = input;
// 	}

// 	public void setOutput(String output) {
// 		isOutput = true;
// 		this.outputName = output;
// 	}

// 	public void delete() {
// 		for (Gate g : this.inputGates) {
// 			g.outputGates.remove(this);
// 		}
// 	}

// 	public int indexOfInput(Gate g) {
// 		for (int index = 0; index < this.numInputs; index++) {
// 			if (this.inputGates[index].equals(g)) {
// 				return index;
// 			}
// 		}
// 		return -1;
// 	}

// 	/**
// 	 * For a single input gate, this will reroute it's input to the passed
// 	 * output and vice versa
// 	 * 
// 	 * @param inputGate
// 	 *            the Input Gate to this.
// 	 * @param outputGate
// 	 *            the Output Gate to this.
// 	 */
// 	public void rerouteSingleInputGates(Gate inputGate, Gate outputGate) {
// 		int index = outputGate.indexOfInput(this);
// 		outputGate.inputGates[index] = inputGate;
// 		outputGate.inputGateIndexes[index] = inputGate.id;
// 		inputGate.outputGates.add(outputGate);
// 	}

// 	public void removeIdentity() {
// 		// Remove self from input gate's list of outputGates
// 		Gate inputGate = this.inputGates[0];
// 		inputGate.outputGates.remove(this);

// 		// for all of self's output gate, reroute input gates
// 		for (Gate n : this.outputGates) {
// 			this.rerouteSingleInputGates(inputGate, n);
// 		}
// 		this.isRemovable = true;
// 	}

// 	public void removeInverse() {
// 		Gate inputGate = this.inputGates[0];
// 		inputGate.outputGates.remove(this);
// 		for (Gate n : this.outputGates) {
// 			this.rerouteSingleInputGates(inputGate, n);
// 			int index = n.indexOfInput(this);
// 			// modify TruthTable
// 			boolean[] newTruthTable = new boolean[n.tableSize];
// 			for (int j = 0; j < n.tableSize; j++) {
// 				if (index == 0) {
// 					if (j % 2 == 0) {
// 						newTruthTable[j] = n.truthTable[j + 1];
// 					} else
// 						newTruthTable[j] = n.truthTable[j - 1];
// 				} else if (index == 1) {
// 					if ((j / 2) % 2 == 0) {
// 						newTruthTable[j] = n.truthTable[j + 2];
// 					} else
// 						newTruthTable[j] = n.truthTable[j - 2];
// 				} else if (index == 2) {
// 					if ((j / 4) % 2 == 0) {
// 						newTruthTable[j] = n.truthTable[j + 4];
// 					} else
// 						newTruthTable[j] = n.truthTable[j - 4];
// 				}
// 			}
// 			n.truthTable = newTruthTable;
// 		}
// 		this.isRemovable = true;
// 	}

// 	//Needs debugging
// 	public void removeContradiction() {
// 		Gate inputGate = this.inputGates[0];
// 		inputGate.outputGates.remove(this);

// 		for (Gate n : this.outputGates) {
// 			int index = n.indexOfInput(this);
// 			boolean newTruthTable[] = new boolean[n.tableSize / 2];
// 			Gate newInputGates[] = new Gate[n.numInputs - 1];
// 			int newInputIndexes[] = new int[n.numInputs - 1];

// 			// Remove self from outputGate's input
// 			for (int j = 0; j < n.numInputs - 1; j++) {
// 				if (j < index) {
// 					newInputGates[j] = n.inputGates[j];
// 					newInputIndexes[j] = n.inputGateIndexes[j];
// 				}
// 				if (j > index) {
// 					newInputGates[j] = n.inputGates[j - 1];
// 					newInputIndexes[j] = n.inputGateIndexes[j - 1];
// 				}
// 			}
// 			// Update Truth Table
// 			for (int j = 0; j < n.tableSize / 2; j++) {
// 				if (index == 2) {
// 					newTruthTable[j] = n.truthTable[j];
// 				} else if (index == 1) {
// 					if (j > 1) {
// 						newTruthTable[j] = n.truthTable[j + 2];
// 					} else {
// 						newTruthTable[j] = n.truthTable[j];
// 					}
// 				} else if (index == 0) {
// 					newTruthTable[j] = n.truthTable[j * 2];
// 				}
// 			}
// 			n.inputGateIndexes = newInputIndexes;
// 			n.inputGates = newInputGates;
// 			n.numInputs -= 1;
// 			n.tableSize /= 2;
// 			n.truthTable = newTruthTable;
// 			if (n.updateBooleanValues()) {
// 				// todo: must remove Gate n... recursive?
// 			}
// 		}
// 		this.isRemovable = true;
// 	}

// 	//Needs Debugging
// 	public void removeTautology() {
// 		Gate inputGate = this.inputGates[0];
// 		inputGate.outputGates.remove(this);

// 		for (Gate n : this.outputGates) {
// 			int index = n.indexOfInput(this);
// 			boolean newTruthTable[] = new boolean[n.tableSize / 2];
// 			Gate newInputGates[] = new Gate[n.numInputs - 1];
// 			int newInputIndexes[] = new int[n.numInputs - 1];

// 			// Remove self from outputGate's input
// 			for (int j = 0; j < n.numInputs - 1; j++) {
// 				if (j < index) {
// 					newInputGates[j] = n.inputGates[j];
// 					newInputIndexes[j] = n.inputGateIndexes[j];
// 				}
// 				if (j > index) {
// 					newInputGates[j] = n.inputGates[j - 1];
// 					newInputIndexes[j] = n.inputGateIndexes[j - 1];
// 				}
// 			}
// 			// Update Truth Table
// 			for (int j = 0; j < n.tableSize / 2; j++) {
// 				if (index == 2) {
// 					newTruthTable[j] = n.truthTable[j + 4];
// 				} else if (index == 1) {
// 					if (j > 1) {
// 						newTruthTable[j] = n.truthTable[j + 4];
// 					} else {
// 						newTruthTable[j] = n.truthTable[j + 2];
// 					}
// 				} else if (index == 0) {
// 					newTruthTable[j] = n.truthTable[j * 2 + 1];
// 				}
// 			}
// 			n.inputGateIndexes = newInputIndexes;
// 			n.inputGates = newInputGates;
// 			n.numInputs -= 1;
// 			n.tableSize /= 2;
// 			n.truthTable = newTruthTable;
// 			if (n.updateBooleanValues()) {
// 				// todo: must remove Gate n... recursive?
// 			}
// 		}
// 		this.isRemovable = true;
// 	}

// 	public String toString() {
// 		String retVal = id + " ";
// 		if (isInput) {
// 			retVal += "input " + inputName;
// 		} else {
// 			if (isOutput) {
// 				retVal += "output ";
// 			}
// 			retVal += "gate arity " + this.numInputs + " table [ ";
// 			for (int i = 0; i < this.tableSize; i++) {
// 				if (truthTable[i]) {
// 					retVal += 1 + " ";
// 				} else {
// 					retVal += 0 + " ";
// 				}
// 			}
// 			retVal += "] inputs [ ";
// 			for (int i = 0; i < this.numInputs; i++) {
// 				retVal += this.inputGates[i].id + " ";
// 			}
// 			retVal += "] ";
// 			if (isOutput) {
// 				retVal += outputName;
// 			}
// 		}
// 		return retVal;
// 	}
// }
