import java.util.ArrayList;
import java.util.LinkedList;

public class OptimizerHeuristic {
	public static void main(String[] args) {
		// testLinearFunction();
		// testInitializeBase();
		// testSum();
		testOptimize();
	}

	public static ArrayList<LinearFunction> optimize(ArrayList<LinearFunction> M) {
		int numBits = M.get(0).getLength();
		ArrayList<LinearFunction> S = initializeBase(numBits);

		int[] distances = new int[M.size()];
		for (int i = 0; i < M.size(); i++)
			distances[i] = M.get(i).getHamming() - 1;

		int count = 0;
		while (sum(distances) != 0) {
			count++;
			System.out.println(count);
			LinearFunction nextBaseElement = pickNextBaseElement(S, M, distances);
			S.add(nextBaseElement);
			for (int i = 0; i < M.size(); i++) {
				LinearFunction fi = M.get(i);
				distances[i] = computeDistance(S, fi);
			}
		}
		return S;
	}

	public static ArrayList<LinearFunction> initializeBase(int numBits) {
		ArrayList<LinearFunction> base = new ArrayList<LinearFunction>();
		for (int i = 0; i < numBits; i++) {
			LinearFunction xi = new LinearFunction(numBits);
			xi.setDigit(i, true);
			base.add(xi);
		}
		return base;
	}

	public static LinearFunction pickNextBaseElement( ArrayList<LinearFunction> S, ArrayList<LinearFunction> M, int[] distances ) {
		// Optimization
		for( LinearFunction fi : S )
			for( LinearFunction fj : S )
				for( int i = 0; i < M.size(); i++) {
					LinearFunction fm = M.get(i);
					LinearFunction candidate = LinearFunction.add(fi, fj);
					if( fm.equals(candidate) && distances[i] != 0 )
						return candidate;
				}
		
		// General selection strategy
		LinearFunction bestF = new LinearFunction( M.get(0).getLength() );
		int[] bestDistances = new int[M.size()];
		int bestSum = Integer.MAX_VALUE;
		for( LinearFunction fi : S )
			for( LinearFunction fj : S ) {
				ArrayList<LinearFunction> newS = new ArrayList<LinearFunction>( S );
				LinearFunction candidate = LinearFunction.add(fi, fj);
				
				newS.add( candidate );
				int[] newDistances = new int[M.size()];
				for( int i = 0; i < M.size(); i++ )
					newDistances[i] = computeDistance( newS, M.get(i) );
				
				int newSum = sum( newDistances );
				if( newSum < bestSum ) {
					bestSum = newSum;
					bestF = candidate;
					bestDistances = newDistances;
				}
				else if( newSum == bestSum ) {
					int normNew = 0;
					for( int i = 0; i < newDistances.length; i++ )
						normNew += newDistances[i] * newDistances[i];
					
					int normBest = 0;
					for( int i = 0; i < bestDistances.length; i++ )
						normBest += bestDistances[i] * bestDistances[i];
					
					if( normNew < normBest ) {
						bestSum = newSum;
						bestF = candidate;
						bestDistances = newDistances;
					}
				}
			}
		return bestF;
	}

	public static int computeDistance(ArrayList<LinearFunction> S,
			LinearFunction fi) {
		LinkedList<LinearFunction> q = new LinkedList<LinearFunction>();
		q.addAll(S);

		int distance = 0;
		int count = 0;
		boolean distanceFound = false;
		while (!distanceFound) {
			count++;
			LinearFunction next = q.poll();

			if (next.equals(fi)) {
				int size = S.size();
				count -= size;
				while (count > 0) {
					distance++;
					size *= S.size();
					count -= size;
				}
				distanceFound = true;
			}

			else
				for (LinearFunction f : S)
					q.add(LinearFunction.add(next, f));
		}
		return distance;
	}

	public static int sum(int[] distances) {
		int sum = 0;
		for (int i = 0; i < distances.length; i++)
			sum += distances[i];
		return sum;
	}

	public static void testOptimize() {
		ArrayList<LinearFunction> M = new ArrayList<LinearFunction>();

		LinearFunction tmp = new LinearFunction(5);
		tmp.setDigit(0, true);
		tmp.setDigit(1, true);
		tmp.setDigit(2, true);
		M.add(tmp);

		tmp = new LinearFunction(5);
		tmp.setDigit(1, true);
		tmp.setDigit(3, true);
		tmp.setDigit(4, true);
		M.add(tmp);

		tmp = new LinearFunction(5);
		tmp.setDigit(0, true);
		tmp.setDigit(2, true);
		tmp.setDigit(3, true);
		tmp.setDigit(4, true);
		M.add(tmp);

		tmp = new LinearFunction(5);
		tmp.setDigit(1, true);
		tmp.setDigit(2, true);
		tmp.setDigit(3, true);
		M.add(tmp);

		tmp = new LinearFunction(5);
		tmp.setDigit(0, true);
		tmp.setDigit(1, true);
		tmp.setDigit(3, true);
		M.add(tmp);

		tmp = new LinearFunction(5);
		tmp.setDigit(1, true);
		tmp.setDigit(2, true);
		tmp.setDigit(3, true);
		tmp.setDigit(4, true);
		M.add(tmp);

		ArrayList<LinearFunction> optM = optimize(M);
		for (LinearFunction f : optM)
			System.out.println(f);
	}

	//
	//
	//
	// Ignore code below this point
	public static void testLinearFunction() {
		LinearFunction t1 = new LinearFunction(8);
		LinearFunction t2 = new LinearFunction(8);

		System.out.println("t1 blank: " + t1);
		System.out.println("t2 blank: " + t2);

		System.out.println("testing setDigit()...");
		t1.setDigit(0, true);
		t1.setDigit(7, true);
		t2.setDigit(0, true);
		System.out.println("t1: " + t1);
		System.out.println("t2: " + t2);

		System.out.println("testing add()...");
		LinearFunction t3 = LinearFunction.add(t1, t2);
		System.out.println("t1: " + t1);
		System.out.println("t2: " + t2);
		System.out.println("t3 = t1 + t2: " + t3);

		System.out.println("testing evaluate()...");
		LinearFunction t4 = new LinearFunction(8);
		t4.setDigit(0, true);
		t4.setDigit(5, true);
		System.out.println("t1: " + t1);
		System.out.println("t4: " + t4);
		System.out.println("t1(t4): " + t1.evaluate(t4));

		System.out.println("testing equals()...");
		System.out.println("t1: " + t1);
		System.out.println("t2: " + t2);
		System.out.println("t1 == t2: " + t1.equals(t2));
		t2.setDigit(7, true);
		System.out.println("new t2: " + t2);
		System.out.println("t1 == t2: " + t1.equals(t2));
	}

	public static void testInitializeBase() {
		ArrayList<LinearFunction> base = initializeBase(8);
		for (LinearFunction fi : base)
			System.out.println(fi);
	}

	public static void testSum() {
		int[] distances = new int[10];
		for (int i = 0; i < 10; i++)
			distances[i] = i;
		System.out.print("[");
		for (int i = 0; i < 9; i++)
			System.out.print(distances[i] + ", ");
		System.out.println(distances[9] + "]");
		System.out.println("sum: " + sum(distances));
	}
}
