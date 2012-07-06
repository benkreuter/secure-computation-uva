
public class LinearFunction {
	private boolean[] digits;
	
	public LinearFunction( int size ) {
		this.digits = new boolean[size];
		for( int i = 0; i < size; i++ )
			this.digits[i] = false;
	}
	
	public int getLength() {
		return this.digits.length;
	}
	
	public void setDigit( int digit, boolean value ) {
		this.digits[digit] = value;
	}
	
	public boolean getDigit( int digit ) {
		return this.digits[digit];
	}
	
	public int getHamming() {
		int hamming = 0;
		for( int i = 0; i < this.getLength(); i++ )
			if( this.getDigit(i) )
				hamming++;
		return hamming;
	}

	public boolean equals( LinearFunction f ) {
		for( int i = 0; i < this.getLength(); i++ )
			if( this.getDigit(i) != f.getDigit(i) )
				return false;
		return true;
	}
	
	public LinearFunction evaluate( LinearFunction inputVector ) {
		LinearFunction retVal = new LinearFunction( this.getLength() );
		for( int i = 0; i < this.getLength(); i++ ) {
			retVal.setDigit( i, this.getDigit(i) && inputVector.getDigit(i) );
		}
		return retVal;
	}
	
	public String toString() {
		String retVal = "";
		for( int i = 0; i < this.digits.length; i++ ) {
			if( this.digits[i] )
				retVal += 1;
			else
				retVal += 0;
		}
		return retVal;
	}
	
	public static LinearFunction add( LinearFunction f1, LinearFunction f2 ) {
		LinearFunction retVal = new LinearFunction( f1.getLength() );
		for( int i = 0; i < f1.getLength(); i++ ) {
			retVal.setDigit( i, f1.getDigit(i) ^ f2.getDigit(i) );
		}
		return retVal;
	}
}
