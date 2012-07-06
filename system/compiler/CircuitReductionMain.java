import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.Scanner;

public class CircuitReductionMain {
	static File inputFile = new File("circuitReduction.txt");
	static ArrayList<Gate> gates = new ArrayList<Gate>();
	static ArrayList<Gate> inputGates = new ArrayList<Gate>();
	static ArrayList<Gate> outputGates = new ArrayList<Gate>();
	static int numberOfGatesRemoved = 0;

	public static void main(String[] args) throws FileNotFoundException {
		readInput();
		removeOutputIndependentGates();
		removeSingleInputGates();
		renumberGates();
		//printGates();
		System.out.println(numberOfGatesRemoved);
	}

	public static void readInput() throws FileNotFoundException {
		Scanner input = new Scanner(System.in);
		while (input.hasNextLine()) {
			String line = input.nextLine();
			if (line.length() > 0) {
				Gate newGate = new Gate(line);
				gates.add(newGate);
				if (newGate.isInput) {
					inputGates.add(newGate);
				} else if (newGate.isOutput) {
					outputGates.add(newGate);
				}
			}
		}

		for (Gate g : gates) {
			if (!g.isInput) {
				for (int i = 0; i < g.numInputs; i++) {
					int neighborIndex = g.inputGateIndexes[i];
					Gate neighbor = gates.get(neighborIndex);
					g.inputGates[i] = neighbor;
					neighbor.addOutput(g);
				}
			}
		}
	}

	public static void removeOutputIndependentGates() {
		LinkedList<Gate> q = new LinkedList<Gate>();
		for (Gate g : outputGates) {
			g.leadsToOutput = true;
			q.add(g);
		}
		// Flood fill the circuit
		while (!q.isEmpty()) {
			Gate current = q.removeFirst();
			if (current.inputGates != null) {
				for (Gate g : current.inputGates) {
					if (!g.leadsToOutput) {
						g.leadsToOutput = true;
						q.add(g);
					}
				}
			}
		}

		for (Gate current : gates) {
			if (!current.leadsToOutput) {
				current.delete();
				current.isRemovable = true;
				numberOfGatesRemoved++;
			}
		}
	}

	public static void removeSingleInputGates() {
		ArrayList<Gate> temp = new ArrayList<Gate>();
		for (int i = gates.size() - 1; i >= 0; i--) {
			Gate g = gates.get(i);
			if (g.isInput || g.isOutput) {
				continue;
			}
			if (g.isIdentity) {
				g.removeIdentity();
				numberOfGatesRemoved++;
			} else if (g.isInverse) {
				g.removeInverse();
				numberOfGatesRemoved++;
			} else if (g.isContradiction) {
				//todo: call function
			} else if (g.isTautology) {
				//todo: call function
			}
		}
		removeRemovableGates();
	}

	public static void removeRemovableGates(){
		ArrayList<Gate> temp = new ArrayList<Gate>();
		for(Gate g : gates){
			if(!g.isRemovable){
				temp.add(g);
			}
		}
		gates = temp;
	}
	public static void renumberGates() {
		int newID = 0;
		for (Gate g : gates) {
			g.id = newID;
			for (Gate n : g.outputGates) {
				n.inputGateIndexes[n.indexOfInput(g)] = newID;
			}
			newID++;
		}
	}

	public static void printGates() {
		for (Gate g : gates) {
			System.out.println(g);
		}
	}
}
