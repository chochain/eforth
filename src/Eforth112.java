import java.util.*;
import java.io.*;
import java.time.LocalTime;

public class Eforth112 {	// ooeforth
	static Scanner in;
	static Stack<Integer> stack      = new Stack<>();
	static Stack<Integer> rstack     = new Stack<>();
	static List<Code>     dictionary = new ArrayList<>();
    
	static boolean compiling = false;
	static int     base      = 10;
	static int     fence     = 0;
	static int     wp, ip;

	public static void main(String args[]) {	// ooeforth 1.12
		System.out.println("ooeForth1.12");
		// code dictionary 
		Code colon  = new Code(":");     colon.token=fence++; dictionary.add(colon);
		Code semi   = new Code(";");     semi.token=fence++;  semi.immediate=true; dictionary.add(semi);
		// stacks
		Code dup    = new Code("dup");   dup.token=fence++; dictionary.add(dup);
		Code over   = new Code("over");  over.token=fence++; dictionary.add(over);
		Code qdup   = new Code("4dup");  qdup.token=fence++; dictionary.add(qdup);
		Code swap   = new Code("swap");  swap.token=fence++; dictionary.add(swap);
		Code rot    = new Code("rot");   rot.token=fence++; dictionary.add(rot);
		Code rrot   = new Code("-rot");  rrot.token=fence++; dictionary.add(rrot);
		Code dswap  = new Code("2swap"); dswap.token=fence++; dictionary.add(dswap);
		Code pick   = new Code("pick"); pick.token=fence++; dictionary.add(pick);
		Code roll   = new Code("roll"); roll.token=fence++; dictionary.add(roll);
		Code ddup   = new Code("2dup"); ddup.token=fence++; dictionary.add(ddup);
		Code dover  = new Code("2over"); dover.token=fence++; dictionary.add(dover);
		Code drop   = new Code("drop"); drop.token=fence++; dictionary.add(drop);
		Code nip    = new Code("nip"); nip.token=fence++; dictionary.add(nip);
		Code ddrop  = new Code("2drop"); ddrop.token=fence++; dictionary.add(ddrop);
		Code tor    = new Code(">r"); tor.token=fence++; dictionary.add(tor);
		Code rfrom  = new Code("r>"); rfrom.token=fence++; dictionary.add(rfrom);
		Code rat    = new Code("r@"); rat.token=fence++; dictionary.add(rat);
		// math
		Code plus   = new Code("+"); plus.token=fence++; dictionary.add(plus);
		Code minus  = new Code("-"); minus.token=fence++; dictionary.add(minus);
		Code mult   = new Code("*");  mult.token=fence++; dictionary.add(mult);
		Code div    = new Code("/"); div.token=fence++; dictionary.add(div);
		Code mod    = new Code("mod"); mod.token=fence++; dictionary.add(mod);
		Code starsl = new Code("*/"); starsl.token=fence++; dictionary.add(starsl);
		Code ssmod  = new Code("*/mod"); ssmod.token=fence++; dictionary.add(ssmod);
		Code and    = new Code("and"); and.token=fence++; dictionary.add(and);
		Code or     = new Code("or"); or.token=fence++; dictionary.add(or);
		Code xor    = new Code("xor"); xor.token=fence++; dictionary.add(xor);
		Code negate = new Code("negate"); negate.token=fence++; dictionary.add(negate);
		// logic
		Code zequal = new Code("0="); zequal.token=fence++; dictionary.add(zequal);
		Code zless  = new Code("0<"); zless.token=fence++; dictionary.add(zless);
		Code zgreat = new Code("0>"); zgreat.token=fence++; dictionary.add(zgreat);
		Code equal  = new Code("="); equal.token=fence++; dictionary.add(equal);
		Code less   = new Code("<"); less.token=fence++; dictionary.add(less);
		Code great  = new Code(">"); great.token=fence++; dictionary.add(great);
		Code nequal = new Code("<>"); nequal.token=fence++; dictionary.add(nequal);
		Code gequal = new Code(">="); gequal.token=fence++; dictionary.add(gequal);
		Code lequal = new Code("<="); lequal.token=fence++; dictionary.add(lequal);
		// output
		Code baseat = new Code("base@"); baseat.token=fence++; dictionary.add(baseat);
		Code basest = new Code("base!"); basest.token=fence++; dictionary.add(basest);
		Code hex    = new Code("hex"); hex.token=fence++; dictionary.add(hex);
		Code decimal= new Code("decimal"); decimal.token=fence++; dictionary.add(decimal);
		Code cr     = new Code("cr"); cr.token=fence++; dictionary.add(cr);
		Code dot    = new Code("."); dot.token=fence++; dictionary.add(dot);
		Code dotr   = new Code(".r"); dotr.token=fence++; dictionary.add(dotr);
		Code udotr  = new Code("u.r"); udotr.token=fence++; dictionary.add(udotr);
		Code key    = new Code("key"); key.token=fence++; dictionary.add(key);
		Code emit   = new Code("emit"); emit.token=fence++; dictionary.add(emit);
		Code space  = new Code("space"); space.token=fence++; dictionary.add(space);
		Code spaces = new Code("spaces"); spaces.token=fence++; dictionary.add(spaces);
		Code rbrac  = new Code("]"); rbrac.token=fence++; dictionary.add(rbrac);
		Code lbrac  = new Code("["); lbrac.token=fence++; dictionary.add(lbrac);
		Code tick   = new Code("'"); tick.token=fence++; dictionary.add(tick);
		Code strq   = new Code("$\""); strq.token=fence++; strq.immediate=true; dictionary.add(strq);
		Code dotq   = new Code(".\""); dotq.token=fence++; dotq.immediate=true; dictionary.add(dotq);
		Code paren  = new Code("("); paren.token=fence++; paren.immediate=true; dictionary.add(paren);
		Code dotpar = new Code(".("); dotpar.token=fence++; dotpar.immediate=true; dictionary.add(dotpar);
		Code bslash = new Code("\\"); bslash.token=fence++; bslash.immediate=true; dictionary.add(bslash);
		// structures
		Code exit   = new Code("exit"); exit.token=fence++; dictionary.add(exit);
		Code exec   = new Code("exec"); exec.token=fence++; dictionary.add(exec);
		Code iff    = new Code("if"); iff.token=fence++; iff.immediate=true; dictionary.add(iff);
		Code elsee  = new Code("else"); elsee.token=fence++; elsee.immediate=true; dictionary.add(elsee);
		Code then   = new Code("then"); then.token=fence++; then.immediate=true; dictionary.add(then);
		Code begin  = new Code("begin"); begin.token=fence++; begin.immediate=true; dictionary.add(begin);
		Code again  = new Code("again"); again.token=fence++; again.immediate=true; dictionary.add(again);
		Code until  = new Code("until"); until.token=fence++; until.immediate=true; dictionary.add(until);
		Code whilee = new Code("while"); whilee.token=fence++; whilee.immediate=true; dictionary.add(whilee);
		Code repeat = new Code("repeat"); repeat.token=fence++; repeat.immediate=true; dictionary.add(repeat);
		Code forr   = new Code("for"); forr.token=fence++; forr.immediate=true; dictionary.add(forr);
		Code next   = new Code("next"); next.token=fence++; next.immediate=true; dictionary.add(next);
		Code aft    = new Code("aft"); aft.token=fence++; aft.immediate=true; dictionary.add(aft);
		// defining words
		Code create = new Code("create"); create.token=fence++; dictionary.add(create);
		Code variable = new Code("variable"); variable.token=fence++; dictionary.add(variable);
		Code constant = new Code("constant"); constant.token=fence++; dictionary.add(constant);
		Code at     = new Code("@"); at.token=fence++; dictionary.add(at);
		Code store  = new Code("!"); store.token=fence++; dictionary.add(store);
		Code plussto= new Code("+!"); plussto.token=fence++; dictionary.add(plussto);
		Code quest  = new Code("?"); quest.token=fence++; dictionary.add(quest);
		Code arrayat= new Code("array@"); arrayat.token=fence++; dictionary.add(arrayat);
		Code arrayst= new Code("array!"); arrayst.token=fence++; dictionary.add(arrayst);
		Code comma  = new Code(","); comma.token=fence++; dictionary.add(comma);
		Code allot  = new Code("allot"); allot.token=fence++; dictionary.add(allot);
		Code does   = new Code("does"); does.token=fence++; dictionary.add(does);
		Code to     = new Code("to"); to.token=fence++; dictionary.add(to);
		Code is     = new Code("is"); is.token=fence++; dictionary.add(is);
		// tools
		Code here   = new Code("here"); here.token=fence++; dictionary.add(here);
		Code words  = new Code("words"); words.token=fence++; dictionary.add(words);
		Code dots   = new Code(".s"); dots.token=fence++; dictionary.add(dots);
		Code see    = new Code("see"); see.token=fence++; dictionary.add(see);
		Code time   = new Code("time"); time.token=fence++; dictionary.add(time);
		Code ms     = new Code("ms"); ms.token=fence++; dictionary.add(ms);
        
		// outer interpreter
		in = new Scanner(System.in);
		String idiom;
		while (!(idiom=in.next()).equals("bye")) {  	// parse input
			Code newWordObject = null;
			for (var w : dictionary) {  				// search dictionary
				if (w.name.equals(idiom)) {
                    newWordObject = w;break;
                };
            }
			if (newWordObject != null) {  				// word found
				if ((!compiling) || newWordObject.immediate) {
					try {                               // execute
                        newWordObject.xt();
                    }
					catch (Exception e) { System.out.print(e); }
                }
				else {  								// or compile
					Code latestWord = dictionary.get(dictionary.size()-1);
					latestWord.addCode(newWordObject);
                }
            }
			else { 
				try {
                    int n=Integer.parseInt(idiom, base); // not word, try number
					if (compiling) {  					// compile integer literal
						Code latestWord = dictionary.get(dictionary.size()-1);
						latestWord.addCode(new Code("dolit", n));
                    }
					else {
                        stack.push(n);
                    }
                }				// or push number on stack
				catch (NumberFormatException  ex) {		// catch number errors
					System.out.println(idiom + " ?");
					compiling = false; stack.clear();
                }
			}
		}
		System.out.println("Thank you.");
		in.close();
	}

	// primitive words
	static class Code {
		public String        name;
		public int           token     = 0;
		public boolean       immediate = false;
        
		public List<Code>    pf  = new ArrayList<>();
		public List<Code>    pf1 = new ArrayList<>();
		public List<Code>    pf2 = new ArrayList<>();
//		public List<Integer> qf = new ArrayList<>(Arrays.asList(-1)) ;
		public List<Integer> qf  = new ArrayList<>() ;
        
		public int     struct = 0;
		public String  literal;
        
		public Code(String n)           { name=n;             }
		public Code(String n, int d)    { name=n;  qf.add(d); }
		public Code(String n, String l) { name=n;  literal=l; }
        
		public void xt() {
			if (lookUp.containsKey(name)) {
				lookUp.get(name).run();
			}
            else {
                rstack.push(wp); rstack.push(ip);
                wp=token; ip = 0;	// wp points to current colon object
                for (Code w:pf) {
                    try {
                        w.xt();
                        ip++;
                    }
                    catch (ArithmeticException e) {}
                }
                ip=rstack.pop(); wp=rstack.pop();
            }
		}
		public void addCode(Code w) { this.pf.add(w); }
		public HashMap<String, Runnable> lookUp = new HashMap<>() {{
                // stacks
                put( "dup",    ()-> { stack.push(stack.peek()); });
                put( "over",   ()-> { stack.push(stack.get(stack.size()-2)); });
                put( "2dup",   ()-> { stack.addAll(stack.subList(stack.size()-2,stack.size())); });
                put( "2over",  ()-> { stack.addAll(stack.subList(stack.size()-4,stack.size()-2)); });
                put( "4dup",   ()-> { stack.addAll(stack.subList(stack.size()-4,stack.size())); });
                put( "swap",   ()-> { stack.add(stack.size()-2,stack.pop()); });
                put( "rot",    ()-> { stack.push(stack.remove(stack.size()-3)); });
                put( "-rot",   ()-> { stack.push(stack.remove(stack.size()-3)); stack.push(stack.remove(stack.size()-3)); });
                put( "2swap",  ()-> { stack.push(stack.remove(stack.size()-4)); stack.push(stack.remove(stack.size()-4)); });
                put( "pick",   ()-> { int i=stack.pop(); int n=stack.get(stack.size()-i-1); stack.push(n); });
                put( "roll",   ()-> { int i=stack.pop(); int n=stack.remove(stack.size()-i-1); stack.push(n); });
                put( "drop",   ()-> { stack.pop(); });
                put( "nip",    ()-> { stack.remove(stack.size()-2); });
                put( "2drop",  ()-> { stack.pop(); stack.pop(); });
                put( ">r",     ()-> { rstack.push(stack.pop()); });
                put( "r>",     ()-> { stack.push(rstack.pop()); });
                put( "r@",     ()-> { stack.push(rstack.peek()); });
                put( "push",   ()-> { rstack.push(stack.pop()); });
                put( "pop",    ()-> { stack.push(rstack.pop()); });
                // math
                put( "+",      ()-> { stack.push(stack.pop()+stack.pop()); });
                put( "-",      ()-> { int n= stack.pop(); stack.push(stack.pop()-n); });
                put( "*",      ()-> { stack.push(stack.pop()*stack.pop()); });
                put( "/",      ()-> { int n= stack.pop(); stack.push(stack.pop()/n); });
                put( "*/",     ()-> { int n=stack.pop(); stack.push(stack.pop()*stack.pop()/n); });
                put( "*/mod",  ()-> { int n=stack.pop(); int m=stack.pop()*stack.pop();
                        stack.push(m%n); stack.push(m/n); });
                put( "mod",    ()-> { int n= stack.pop(); stack.push(stack.pop()%n); });
                put( "and",    ()-> { stack.push(stack.pop()&stack.pop()); });
                put( "or",     ()-> { stack.push(stack.pop()|stack.pop()); });
                put( "xor",    ()-> { stack.push(stack.pop()^stack.pop()); });
                put( "negate", ()-> { stack.push(-stack.pop()); });
                // logic
                put( "0=",     ()-> { stack.push((stack.pop()==0)?-1:0); });
                put( "0<",     ()-> { stack.push((stack.pop()<0)?-1:0); });
                put( "0>",     ()-> { stack.push((stack.pop()>0)?-1:0); });
                put( "=",      ()-> { int n= stack.pop(); stack.push((stack.pop()==n)?-1:0); });
                put( ">",      ()-> { int n= stack.pop(); stack.push((stack.pop()>n )?-1:0); });
                put( "<",      ()-> { int n= stack.pop(); stack.push((stack.pop()<n )?-1:0); });
                put( "<>",     ()-> { int n= stack.pop(); stack.push((stack.pop()!=n)?-1:0); });
                put( ">=",     ()-> { int n= stack.pop(); stack.push((stack.pop()>=n)?-1:0); });
                put( "<=",     ()-> { int n= stack.pop(); stack.push((stack.pop()<=n)?-1:0); });
                // output
                put( "base@",  ()-> { stack.push(base); });
                put( "base!",  ()-> { base = stack.pop(); });
                put( "hex",    ()-> { base = 16; });
                put( "decimal",()-> { base = 10; });
                put( "cr",     ()-> { System.out.println(); });
                put( ".",      ()-> { System.out.print(Integer.toString(stack.pop(),base)+" "); });
                put( ".r",     ()-> {
                        int    n = stack.pop();
                        String s = Integer.toString(stack.pop(),base);
                        for (int i=0; i+s.length()<n; i++) System.out.print(" ");
                        System.out.print(s+" ");
                    });
                put( "u.r",    ()-> {
                        int    n = stack.pop();
                        String s = Integer.toString(stack.pop()&0x7fffffff,base);
                        for (int i=0; i+s.length()<n; i++) System.out.print(" ");
                        System.out.print(s+" ");
                    });
                put( "key",    ()-> { stack.push((int) in.next().charAt(0)); });
                put( "emit",   ()-> { System.out.print(Character.toChars( stack.pop())); });
                put( "space",  ()-> { System.out.print(" "); });
                put( "spaces", ()-> {
                        int n=stack.pop();
                        for (int i=0; i<n; i++) System.out.print(" ");
                    });
                // literals
                put( "[",      ()-> { compiling = false; });
                put( "]",      ()-> { compiling = true; });
                put( "'",      ()-> {
                        String  s    = in.next();
                        boolean found=false;
                        for (var w:dictionary) {
                            if (s.equals(w.name)) {
                                stack.push(w.token);
                                found = true;break;
                            }
                        }
                        if (!found) stack.push(-1);
                    });
                put( "dolit",  ()-> {	stack.push(qf.get(0)); });			// integer literal
                put( "dostr",  ()-> {	stack.push(token); });			// string literals
                put( "$\"",    ()-> {  // -- w a
                        var d = in.delimiter();
                        in.useDelimiter("\"");			// need fix
                        String s=in.next();
                        Code last = dictionary.get(dictionary.size()-1);
                        last.addCode(new Code("dostr",s));			// literal=s
                        in.useDelimiter(d); in.next();
                        stack.push(last.token); stack.push(last.pf.size()-1);
                    });
                put( "dotstr", ()-> { System.out.print(literal); });
                put( ".\"", ()-> {
                        var d = in.delimiter();
                        in.useDelimiter("\"");			// need fix
                        String s=in.next();
                        Code last = dictionary.get(dictionary.size()-1);
                        last.addCode(new Code("dotstr",s));		// literal=s
                        in.useDelimiter(d); in.next();
                    });
                put( "(",      ()-> { 
                        var d = in.delimiter();
                        in.useDelimiter("\\)");
                        String s=in.next();
                        in.useDelimiter(d); in.next();
                    });
                put( ".(",     ()-> { 
                        var d = in.delimiter();
                        in.useDelimiter("\\)"); system.out.print(in.next());
                        in.useDelimiter(d); in.next();
                    });
                put( "\\",     ()-> { 
                        var d = in.delimiter();
                        in.useDelimiter("\n"); in.next();
                        in.useDelimiter(d); in.next();
                    });
                // structure: if else then
                put( "branch", ()-> {
                        if(!(stack.pop()==0)) {
                            for (var w:pf) w.xt();
                        }
                        else {
                            for (var w:pf1) w.xt();
                        }
                    });
                put( "if",     ()-> { 
                        Code last = dictionary.get(dictionary.size()-1);
                        last.addCode(new Code("branch"));
                        dictionary.add(new Code("temp"));
                    });
                put( "else",   ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        last.pf.get(last.pf.size()-1).pf.addAll(temp.pf);
                        temp.pf.clear();
                        last.pf.get(last.pf.size()-1).struct=1;
                    });
                put( "then",   ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        if (last.pf.get(last.pf.size()-1).struct==0) {
                            last.pf.get(last.pf.size()-1).pf.addAll(temp.pf);
                            dictionary.remove(dictionary.size()-1);
                        } else {
                            last.pf.get(last.pf.size()-1).pf1.addAll(temp.pf);
                            if (last.pf.get(last.pf.size()-1).struct==1) {
                                dictionary.remove(dictionary.size()-1);
                            }
                            else temp.pf.clear();
                        }
                    });
                // loops
                put( "loops",  ()-> {
                        if (struct==1) {	// again
                            while(true) {for (var w:pf) w.xt(); }}
                        if (struct==2) {	// while repeat
                            while (true) {
                                for (var w:pf) w.xt();
                                if (stack.pop()==0) break;
                                for (var w:pf1) w.xt(); }
                        } else {
                            while(true) {	// until
                                for (var w:pf) w.xt();
                                if(stack.pop()!=0) break; }
                        }
                    });
                put( "begin",  ()-> { 
                        Code last = dictionary.get(dictionary.size()-1);
                        last.addCode(new Code("loops"));
                        dictionary.add(new Code("temp"));
                    });
                put( "while",  ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        last.pf.get(last.pf.size()-1).pf.addAll(temp.pf);
                        temp.pf.clear();
                        last.pf.get(last.pf.size()-1).struct=2;
                    });
                put( "repeat", ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        last.pf.get(last.pf.size()-1).pf1.addAll(temp.pf);
                        dictionary.remove(dictionary.size()-1);
                    });
                put( "again",  ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        last.pf.get(last.pf.size()-1).pf.addAll(temp.pf);
                        last.pf.get(last.pf.size()-1).struct=1;
                        dictionary.remove(dictionary.size()-1);
                    });
                put( "until",  ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        last.pf.get(last.pf.size()-1).pf.addAll(temp.pf);
                        dictionary.remove(dictionary.size()-1);
                    });
                // for next
                put( "cycles", ()-> { int i=0;
                        if (struct==0) {
                            while(true){
                                for (var w:pf) w.xt();
                                i=rstack.pop(); i--;
                                if (i<0) break;
                                rstack.push(i);
                            }
                        }
                        else {
                            if (struct>0) {
                                for (var w:pf) w.xt();
                                while(true){
                                    for (var w:pf2) w.xt();
                                    i=rstack.pop(); i--;
                                    if (i<0) break;
                                    rstack.push(i);
                                    for (var w:pf1) w.xt();
                                }
                            }
                        }
                    });
                put( "for",  ()-> {
                        Code last = dictionary.get(dictionary.size()-1);
                        last.addCode(new Code(">r"));
                        last.addCode(new Code("cycles"));
                        dictionary.add(new Code("temp"));
                    });
                put( "aft",  ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        last.pf.get(last.pf.size()-1).pf.addAll(temp.pf);
                        temp.pf.clear();
                        last.pf.get(last.pf.size()-1).struct=3;
                    });
                put( "next", ()-> {
                        Code last = dictionary.get(dictionary.size()-2);
                        Code temp = dictionary.get(dictionary.size()-1);
                        if (last.pf.get(last.pf.size()-1).struct==0) 
                            last.pf.get(last.pf.size()-1).pf.addAll(temp.pf);
                        else last.pf.get(last.pf.size()-1).pf2.addAll(temp.pf);
                        dictionary.remove(dictionary.size()-1);
                    });
                // defining words
                put( "exit", ()-> { throw new ArithmeticException(); });								// marker to exit interpreter
                put( "exec", ()-> { int n=stack.pop();dictionary.get(n).xt(); });
                put( ":",    ()-> {          								// -- box
                        String s = in.next();
                        dictionary.add(new Code(s));
                        Code last = dictionary.get(dictionary.size()-1);
                        last.token=fence++;
                        compiling = true;
                    });
                put( ";", ()-> {          								
                        Code last = dictionary.get(dictionary.size()-1);
                        compiling = false;
                    });
                put( "docon", ()-> { stack.push(qf.get(0)); });			// integer literal
                put( "dovar", ()-> { stack.push(token); });			// string literals
                put( "create", ()-> {
                        String s = in.next();
                        dictionary.add(new Code(s));
                        Code last = dictionary.get(dictionary.size()-1);
                        last.token=fence++;
                        last.addCode(new Code("dovar",0));
                        last.pf.get(0).token=last.token;
                        last.pf.get(0).qf.remove(0);
                    });
                put( "variable", ()-> {  
                        String s = in.next();
                        dictionary.add(new Code(s));
                        Code last = dictionary.get(dictionary.size()-1);
                        last.token=fence++;
                        last.addCode(new Code("dovar",0));
                        last.pf.get(0).token=last.token;
                    });
                put( "constant", ()-> {   // n --
                        String s = in.next();
                        dictionary.add(new Code(s));
                        Code last = dictionary.get(dictionary.size()-1);
                        last.token=fence++;
                        last.addCode(new Code("docon",stack.pop()));
                        last.pf.get(0).token=last.token;
                    });
                put( "@", ()-> {   // w -- n
                        Code last = dictionary.get(stack.pop());
                        stack.push(last.pf.get(0).qf.get(0));
                    });
                put( "!", ()-> {   // n w -- 
                        Code last = dictionary.get(stack.pop());
                        last.pf.get(0).qf.set(0,stack.pop());
                    });
                put( "+!", ()-> {   // n w -- 
                        Code last = dictionary.get(stack.pop());
                        int n=last.pf.get(0).qf.get(0); n+= stack.pop();
                        last.pf.get(0).qf.set(0,n);
                    });
                put( "?", ()-> {   // w -- 
                        Code last = dictionary.get(stack.pop());
                        System.out.print(last.pf.get(0).qf.get(0));
                    });
                put( "array@", ()-> {   // w a -- n
                        int a = stack.pop();
                        Code last = dictionary.get(stack.pop());
                        stack.push(last.pf.get(0).qf.get(a));
                    });
                put( "array!", ()-> {   // n w a -- 
                        int a = stack.pop();
                        Code last = dictionary.get(stack.pop());
                        last.pf.get(0).qf.set(a,stack.pop());
                    });
                put( ",", ()-> {  // n --
                        Code last = dictionary.get(dictionary.size()-1);
                        last.pf.get(0).qf.add(stack.pop());
                    });
                put( "allot", ()-> {   // n --
                        int n = stack.pop(); 
                        Code last = dictionary.get(dictionary.size()-1);
                        for (int i=0; i<n; i++) last.pf.get(0).qf.add(0);
                    });
                put( "does", ()-> {  // n --
                        Code last = dictionary.get(dictionary.size()-1);
                        Code source = dictionary.get(wp);
                        last.pf.addAll(source.pf.subList(ip+2,source.pf.size()));
                    });
                put( "to", ()-> {   									    // n -- , compile only 
                        Code last = dictionary.get(wp);	ip++;		        // current colon word
                        last.pf.get(ip++).pf.get(0).qf.set(0,stack.pop());	// next constant
                    });
                put( "is", ()-> {   									    // w -- , execute only
                        Code source = dictionary.get(stack.pop());	        // source word
                        String s = in.next(); boolean found=false;
                        for (var w:dictionary) {
                            if (s.equals(w.name)) { 				        // target word
                                Code target = dictionary.get(w.token); 
                                target.pf=source.pf; 				        // copy pf 
                                found = true;break; }}
                        if (!found) System.out.print(s+" ?");
                    });
                // tools
                put( "here",  ()-> { stack.push(fence); });
                put( "words", ()-> {
                        int i=0;
                        for (var w:dictionary) {
                            System.out.print(w.name + " ");
                            i++;
                            if (i>15) {
                                System.out.println();
                                i=0;
                            }
                        }
                    });
                put( ".s",   ()-> { for (int n:stack) System.out.print(Integer.toString(n,base)+" "); });
                put( "see",  ()-> { String s = in.next(); boolean found=false;
                        for (var word:dictionary) {
                            if (s.equals(word.name)) { 
                                System.out.println(word.name+", "+word.token+", "+word.qf.toString());
                                for (var w:word.pf) System.out.print(w.name+", "+w.token+", "+w.qf.toString()+"| ");       
                                found = true; break; }
                        }
                        if (!found) System.out.print(s+" ?");
                    });
                put( "time", ()-> { 
                        LocalTime now = LocalTime.now();
                        System.out.println(now);
                    });
                put( "ms", ()-> {  // n --
                        try { Thread.sleep(stack.pop()); } 
                        catch (Exception e) { System.out.println(e); }
                    });
			}};
    }
}

