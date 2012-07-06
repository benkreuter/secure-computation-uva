(defstruct gate
  (table 0 :read-only t)
  (inputs (cons nil nil))
  )

(defun gate-value (gate x y)
  (if (= x 0)
      (if (= y 0)
          (mod (gate-table gate) 2)
        (mod (floor (/ (gate-table gate) 2)) 2)
        )
    (if (= y 0)
        (mod (floor (/ (gate-table gate) 4)) 2)
      (mod (floor (/ (gate-table gate) 8)) 2)
      )
    )
  )

(defun gate-output-function (g)
  (let ((gate g))
    (lambda () (gate-value gate 
                           (funcall (car (gate-inputs gate)))
                           (funcall (cdr (gate-inputs gate)))
                           )
      )
    )
  )

(defun connect-gates (x y z)
  (setf (gate-inputs x) (cons
                         (gate-output-function y)
                         (gate-output-function z)
                         )
        )
  )

(defun pow (x y) (if (= y 0) 1
                   (* x (pow x (- y 1)))
                   )
  )

(defun base-two (x n)
  (let ((ret (make-array n)))
    (loop for i from 0 to (- n 1) do
          (setf (aref ret i)
                (mod (floor (/ x (pow 2 i))) 2)
                )
          )
    ret
    )
  )


(defun run-test ()
  (let ((inputs (make-array 2))
        (x (make-gate :table 9))
        (y (make-gate :table 6))
        (z (make-gate :table 6))
        )
    (setf (gate-inputs x) (cons (gate-output-function y)
                                (gate-output-function z))
          )
    (setf (gate-inputs y) (cons (lambda () (aref inputs 0))
                                (lambda () (aref inputs 1)))
          )
    (setf (gate-inputs z) (cons (lambda () (aref inputs 0))
                                (lambda () (aref inputs 1)))
          )
    (setf (symbol-function 'val) (gate-output-function x))

    (loop for i from 0 to 3 do
          (setf inputs (base-two i 2))
          (print (val))
          )
    )
  )